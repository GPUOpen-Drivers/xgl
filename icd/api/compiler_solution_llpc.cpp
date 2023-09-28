/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
***********************************************************************************************************************
* @file  compiler_solution_llpc.cpp
* @brief Contains implementation of CompilerSolutionLlpc
***********************************************************************************************************************
*/
#include "include/compiler_solution_llpc.h"
#include "include/vk_device.h"
#include "include/vk_physical_device.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_cache.h"
#include "include/graphics_pipeline_common.h"

#include <inttypes.h>

namespace vk
{

// =====================================================================================================================
static const char* GetCacheAccessString(
    Llpc::CacheAccessInfo cacheAccess)
{
    const char* pStr = "";
    switch (cacheAccess)
    {
    case Llpc::CacheNotChecked:
        pStr = "CacheNotChecked";
        break;
    case Llpc::CacheMiss:
        pStr = "CacheMiss";
        break;
    case Llpc::CacheHit:
        pStr = "CacheHit";
        break;
    case Llpc::InternalCacheHit:
        pStr = "InternalCacheHit";
        break;
    case Llpc::PartialPipelineHit:
        pStr = "PartialPipelineHit";
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }
    return pStr;
}

// =====================================================================================================================
CompilerSolutionLlpc::CompilerSolutionLlpc(
    PhysicalDevice* pPhysicalDevice)
    :
    CompilerSolution(pPhysicalDevice),
    m_pLlpc(nullptr)
{

}

// =====================================================================================================================
CompilerSolutionLlpc::~CompilerSolutionLlpc()
{
    VK_ASSERT(m_pLlpc == nullptr);
}

// =====================================================================================================================
// Initialize CompilerSolutionLlpc class
VkResult CompilerSolutionLlpc::Initialize(
    Vkgc::GfxIpVersion    gfxIp,
    Pal::GfxIpLevel       gfxIpLevel,
    PipelineBinaryCache*  pCache)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    Vkgc::ICache* pInternalCache = nullptr;
    if (pCache != nullptr)
    {
        if (settings.shaderCacheMode != ShaderCacheDisable)
        {
            pInternalCache = pCache->GetCacheAdapter();
        }
    }

    VkResult result = CompilerSolution::Initialize(gfxIp, gfxIpLevel, pCache);

    if (result == VK_SUCCESS)
    {
        result = CreateLlpcCompiler(pInternalCache);
    }

    return result;
}

// =====================================================================================================================
// Destroy CompilerSolutionLlpc class
void CompilerSolutionLlpc::Destroy()
{
    if (m_pLlpc)
    {
        m_pLlpc->Destroy();
        m_pLlpc = nullptr;
    }
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult CompilerSolutionLlpc::BuildShaderModule(
    const Device*                pDevice,
    VkShaderModuleCreateFlags    flags,
    VkShaderModuleCreateFlags    internalShaderFlags,
    size_t                       codeSize,
    const void*                  pCode,
    const bool                   adaptForFastLink,
    bool                         isInternal,
    ShaderModuleHandle*          pShaderModule,
    const PipelineOptimizerKey&  profileKey)
{
    VkResult result = VK_SUCCESS;
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    // Build LLPC shader module
    Llpc::ShaderModuleBuildInfo  moduleInfo = {};
    Llpc::ShaderModuleBuildOut   buildOut = {};
    void* pShaderMemory = nullptr;

    moduleInfo.pInstance      = pInstance;
    moduleInfo.pfnOutputAlloc = AllocateShaderOutput;
    moduleInfo.pUserData      = &pShaderMemory;
    moduleInfo.shaderBin.pCode    = pCode;
    moduleInfo.shaderBin.codeSize = codeSize;

    auto pPipelineCompiler = m_pPhysicalDevice->GetCompiler();
    pPipelineCompiler->ApplyPipelineOptions(pDevice, 0, &moduleInfo.options.pipelineOptions
    );

#if VKI_RAY_TRACING
    if ((internalShaderFlags & VK_INTERNAL_SHADER_FLAGS_RAY_TRACING_INTERNAL_SHADER_BIT) != 0)
    {
        moduleInfo.options.pipelineOptions.internalRtShaders = true;
    }
#endif

    Vkgc::Result llpcResult = m_pLlpc->BuildShaderModule(&moduleInfo, &buildOut);

    if ((llpcResult == Vkgc::Result::Success) || (llpcResult == Vkgc::Result::Delayed))
    {
        pShaderModule->pLlpcShaderModule = buildOut.pModuleData;
        VK_ASSERT(pShaderMemory == pShaderModule->pLlpcShaderModule);
    }
    else
    {
        // Clean up if fail
        pInstance->FreeMem(pShaderMemory);
        if (llpcResult == Vkgc::Result::ErrorOutOfMemory)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return result;
}

// =====================================================================================================================
// Frees shader module memory
void CompilerSolutionLlpc::FreeShaderModule(ShaderModuleHandle* pShaderModule)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    pInstance->FreeMem(pShaderModule->pLlpcShaderModule);
}

// =====================================================================================================================
// Creates graphics pipeline binary.
VkResult CompilerSolutionLlpc::CreateGraphicsPipelineBinary(
    Device*                           pDevice,
    uint32_t                          deviceIdx,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    size_t*                           pPipelineBinarySize,
    const void**                      ppPipelineBinary,
    Vkgc::PipelineShaderInfo**        ppShadersInfo,
    void*                             pPipelineDumpHandle,
    uint64_t                          pipelineHash,
    Util::MetroHash::Hash*            pCacheId,
    int64_t*                          pCompileTime)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result = VK_SUCCESS;

    // Build the LLPC pipeline
    Llpc::GraphicsPipelineBuildOut pipelineOut = {};

    const auto& pipelineProfileKey = *pCreateInfo->pPipelineProfileKey;

    int64_t startTime = Util::GetPerfCpuTime();
    auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
    pPipelineBuildInfo->pInstance      = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
    pPipelineBuildInfo->iaState.deviceIndex = deviceIdx;

    // By default the client hash provided to PAL is more accurate than the one used by pipeline
    // profiles.
    //
    // Optionally (based on panel setting), these can be set to temporarily match by devs.  This
    // can be useful when other tools (such as PAL's profiling layer) are used to measure shaders
    // while building a pipeline profile which uses the profile hash.
    if (settings.pipelineUseProfileHashAsClientHash)
    {
        for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
        {
            ppShadersInfo[stage]->options.clientHash.lower = pipelineProfileKey.pShaders[stage].codeHash.lower;
            ppShadersInfo[stage]->options.clientHash.upper = pipelineProfileKey.pShaders[stage].codeHash.upper;
        }
    }

    for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; ++stage)
    {
        if (ppShadersInfo[stage]->pModuleData != nullptr)
        {
            const auto* pModuleData =
                reinterpret_cast<const Vkgc::ShaderModuleData*>(ppShadersInfo[stage]->pModuleData);
#if VKI_RAY_TRACING
            pCreateInfo->pBinaryMetadata->rayQueryUsed  |= pModuleData->usage.enableRayQuery;
#endif
            pCreateInfo->pBinaryMetadata->pointSizeUsed           |= pModuleData->usage.usePointSize;
            pCreateInfo->pBinaryMetadata->shadingRateUsedInShader |= pModuleData->usage.useShadingRate;
        }
    }

    Vkgc::Result llpcResult = Vkgc::Result::Success;

    bool isEarlyCompiler = false;
    for (uint32_t i = 0; i < GraphicsLibraryCount; ++i)
    {
        if (pCreateInfo->earlyElfPackage[i].pCode != nullptr)
        {
            isEarlyCompiler = true;
            break;
        }
    }

    if ((isEarlyCompiler == false) || pCreateInfo->linkTimeOptimization)
    {
        llpcResult = m_pLlpc->BuildGraphicsPipeline(pPipelineBuildInfo, &pipelineOut, pPipelineDumpHandle);
    }
    else
    {
        BinaryData elfPackage[Vkgc::UnlinkedShaderStage::UnlinkedStageCount] = {};

        if (pCreateInfo->earlyElfPackage[GraphicsLibraryPreRaster].pCode != nullptr)
        {
            elfPackage[UnlinkedStageVertexProcess] = pCreateInfo->earlyElfPackage[GraphicsLibraryPreRaster];
        }

        if (pCreateInfo->earlyElfPackage[GraphicsLibraryFragment].pCode != nullptr)
        {
            elfPackage[UnlinkedStageFragment] = pCreateInfo->earlyElfPackage[GraphicsLibraryFragment];
        }

        CompilerSolution::DisableNggCulling(&pCreateInfo->pipelineInfo.nggState);

        llpcResult = m_pLlpc->buildGraphicsPipelineWithElf(&pCreateInfo->pipelineInfo, &pipelineOut, elfPackage);

        // Early compile failure in some cases is expected.
        if (llpcResult != Vkgc::Result::Success)
        {
            llpcResult = m_pLlpc->BuildGraphicsPipeline(pPipelineBuildInfo, &pipelineOut, pPipelineDumpHandle);
        }
    }

    pCreateInfo->pipelineFeedback = {};
    memset(pCreateInfo->stageFeedback, 0, sizeof(pCreateInfo->stageFeedback));
    if (llpcResult != Vkgc::Result::Success)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        *ppPipelineBinary   = pipelineOut.pipelineBin.pCode;
        *pPipelineBinarySize = pipelineOut.pipelineBin.codeSize;
        if (pipelineOut.pipelineCacheAccess != Llpc::CacheAccessInfo::CacheNotChecked)
        {
            pCreateInfo->pipelineFeedback.feedbackValid = true;
            pCreateInfo->pipelineFeedback.hitApplicationCache =
                (pipelineOut.pipelineCacheAccess == Llpc::CacheAccessInfo::CacheHit);
        }
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->task,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageTask);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->vs,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageVertex);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->tcs,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageTessControl);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->tes,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageTessEval);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->gs,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageGeometry);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->mesh,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageMesh);
        UpdateStageCreationFeedback(pCreateInfo->stageFeedback,
                                    pPipelineBuildInfo->fs,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageFragment);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            char                      extraInfo[256] = {};
            const ShaderOptimizerKey* pShaderKey     = pipelineProfileKey.pShaders;

            Util::Snprintf(extraInfo, sizeof(extraInfo),
                "\n;CacheHitInfo: PipelineCache: %s ", GetCacheAccessString(pipelineOut.pipelineCacheAccess));
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);

            for (uint32_t i = 0; i < ShaderStageGfxCount; i++)
            {
                if (ppShadersInfo[i]->pModuleData != nullptr)
                {
                    const char* pName = GetShaderStageName(static_cast<ShaderStage>(i));
                    Util::Snprintf(
                        extraInfo,
                        sizeof(extraInfo),
                        "| %s Cache: %s ", pName, GetCacheAccessString(pipelineOut.stageCacheAccesses[i]));
                    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
                }
            }
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n");

            for (uint32_t i = 0; i < GraphicsLibraryCount; i++)
            {
                if (pCreateInfo->libraryHash[i] != 0)
                {
                    const char* pName = GetGraphicsLibraryName(static_cast<GraphicsLibraryType>(i));
                    Util::Snprintf(
                        extraInfo,
                        sizeof(extraInfo),
                        ";%s GraphicsPipelineLibrary Hash: 0x%016" PRIX64 "\n",
                        pName,
                        pCreateInfo->libraryHash[i]);
                    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
                }
            }

            Util::Snprintf(extraInfo, sizeof(extraInfo), "\n;PipelineOptimizer\n");
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);

            for (uint32_t i = 0; i < pipelineProfileKey.shaderCount; i++)
            {
                if (pShaderKey[i].codeHash.upper || pShaderKey[i].codeHash.lower)
                {
                    const char* pName = GetShaderStageName(static_cast<ShaderStage>(i));
                    Util::Snprintf(
                        extraInfo,
                        sizeof(extraInfo),
                        ";%s Shader Profile Key: 0x%016" PRIX64 "%016" PRIX64 ",\n",
                        pName,
                        pShaderKey[i].codeHash.upper,
                        pShaderKey[i].codeHash.lower);
                    Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
                }
            }
        }
    }

    *pCompileTime = Util::GetPerfCpuTime() - startTime;

    return result;
}

// =====================================================================================================================
// Creates color export binary.
VkResult CompilerSolutionLlpc::CreateColorExportBinary(
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    void*                             pPipelineDumpHandle,
    Vkgc::BinaryData*                 pOutputPackage)
{
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto pInstance                   = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result                            = VK_SUCCESS;
    Llpc::GraphicsPipelineBuildOut pipelineOut = {};

    auto pPipelineBuildInfo                 = &pCreateInfo->pipelineInfo;
    pPipelineBuildInfo->pInstance           = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc      = AllocateShaderOutput;

    VK_ASSERT(pCreateInfo->pBinaryMetadata->pFsOutputMetaData != nullptr);
    Vkgc::Result llpcResult = m_pLlpc->BuildColorExportShader(pPipelineBuildInfo,
                                                              pCreateInfo->pBinaryMetadata->pFsOutputMetaData,
                                                              &pipelineOut,
                                                              pPipelineDumpHandle);

    if (llpcResult == Vkgc::Result::Success)
    {
        pOutputPackage->pCode    = pipelineOut.pipelineBin.pCode;
        pOutputPackage->codeSize = pipelineOut.pipelineBin.codeSize;
        if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
        {
            Vkgc::IPipelineDumper::DumpPipelineBinary(pPipelineDumpHandle, m_gfxIp, &pipelineOut.pipelineBin);
        }
    }
    else
    {
        result = (llpcResult == Vkgc::Result::RequireFullPipeline) ? VK_PIPELINE_COMPILE_REQUIRED :
                                                                     VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Build ElfPackage for a specific shader module based on pipeline information
VkResult CompilerSolutionLlpc::CreateGraphicsShaderBinary(
    const Device*                     pDevice,
    PipelineCache*                    pPipelineCache,
    const ShaderStage                 stage,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    void*                             pPipelineDumpHandle,
    ShaderModuleHandle*               pShaderModule)
{
    VkResult result = VK_SUCCESS;
    Util::MetroHash::Hash cacheId = {};
    GraphicsLibraryType gplType = GetGraphicsLibraryType(stage);

    bool hitCache = false;
    if (pCreateInfo->earlyElfPackage[gplType].pCode == nullptr)
    {
        if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
        {
            Vkgc::BinaryData elfPackage = {};
            Util::MetroHash128 hasher;
            hasher.Update(pCreateInfo->libraryHash[gplType]);
            hasher.Update(m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash());
            hasher.Finalize(cacheId.bytes);
            auto pAppCache = pPipelineCache->GetPipelineCache();
            hitCache = (pAppCache->LoadPipelineBinary(&cacheId, &elfPackage.codeSize, &elfPackage.pCode)
                == Util::Result::Success);
            pShaderModule->elfPackage = elfPackage;

            // Update the shader feedback
            PipelineCreationFeedback* pStageFeedBack = &pCreateInfo->stageFeedback[stage];
            pStageFeedBack->feedbackValid = true;
            pStageFeedBack->hitApplicationCache = hitCache;
        }

        if (hitCache == false)
        {
            // Build the LLPC pipeline
            Llpc::GraphicsPipelineBuildOut  pipelineOut = {};
            Vkgc::UnlinkedShaderStage unlinkedStage = UnlinkedStageCount;

            // Belong to vertexProcess stage before fragment
            if (stage < ShaderStage::ShaderStageFragment)
            {
                unlinkedStage = UnlinkedShaderStage::UnlinkedStageVertexProcess;
            }
            else if (stage == ShaderStage::ShaderStageFragment)
            {
                unlinkedStage = UnlinkedShaderStage::UnlinkedStageFragment;
            }

            auto llpcResult = m_pLlpc->buildGraphicsShaderStage(
                &pCreateInfo->pipelineInfo,
                &pipelineOut,
                unlinkedStage,
                pPipelineDumpHandle);
            if (llpcResult == Vkgc::Result::Success)
            {
                if (stage == ShaderStage::ShaderStageFragment)
                {
                    pCreateInfo->pBinaryMetadata->pFsOutputMetaData    = pipelineOut.fsOutputMetaData;
                    pCreateInfo->pBinaryMetadata->fsOutputMetaDataSize = pipelineOut.fsOutputMetaDataSize;
                    pipelineOut.pipelineBin.codeSize += pipelineOut.fsOutputMetaDataSize;
                }

                pShaderModule->elfPackage = pipelineOut.pipelineBin;
                if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCache() != nullptr))
                {
                    pPipelineCache->GetPipelineCache()->StorePipelineBinary(
                        &cacheId, pipelineOut.pipelineBin.codeSize, pipelineOut.pipelineBin.pCode);
                }
            }
            else if (llpcResult != Vkgc::Result::RequireFullPipeline)
            {
                result = (llpcResult == Vkgc::Result::ErrorOutOfMemory) ? VK_ERROR_OUT_OF_HOST_MEMORY :
                                                                          VK_ERROR_INITIALIZATION_FAILED;
            }

            if (llpcResult == Vkgc::Result::Success)
            {
                pCreateInfo->earlyElfPackage[gplType]     = pShaderModule->elfPackage;
                pCreateInfo->earlyElfPackageHash[gplType] = cacheId;

                if (stage == ShaderStage::ShaderStageFragment)
                {
                    const auto* pModuleData =
                        reinterpret_cast<const Vkgc::ShaderModuleData*>(pCreateInfo->pipelineInfo.fs.pModuleData);
                    pCreateInfo->pBinaryMetadata->needsSampleInfo = pModuleData->usage.useSampleInfo;
                }

                if (pPipelineDumpHandle != nullptr)
                {
                    Vkgc::IPipelineDumper::DumpPipelineBinary(
                        pPipelineDumpHandle, m_gfxIp, &pCreateInfo->earlyElfPackage[gplType]);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Creates compute pipeline binary.
VkResult CompilerSolutionLlpc::CreateComputePipelineBinary(
    Device*                          pDevice,
    uint32_t                         deviceIdx,
    PipelineCache*                   pPipelineCache,
    ComputePipelineBinaryCreateInfo* pCreateInfo,
    size_t*                          pPipelineBinarySize,
    const void**                     ppPipelineBinary,
    void*                            pPipelineDumpHandle,
    uint64_t                         pipelineHash,
    Util::MetroHash::Hash*           pCacheId,
    int64_t*                         pCompileTime)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    AppProfile             appProfile = m_pPhysicalDevice->GetAppProfile();

    Vkgc::ComputePipelineBuildInfo* pPipelineBuildInfo = &pCreateInfo->pipelineInfo;

    VkResult result = VK_SUCCESS;

    int64_t startTime = Util::GetPerfCpuTime();

    const auto& pipelineProfileKey = *pCreateInfo->pPipelineProfileKey;

    // Build the LLPC pipeline
    Llpc::ComputePipelineBuildOut pipelineOut = {};

    // Fill pipeline create info for LLPC
    pPipelineBuildInfo->pInstance      = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;

    // Force enable automatic workgroup reconfigure.
    if (appProfile == AppProfile::DawnOfWarIII)
    {
        pPipelineBuildInfo->options.reconfigWorkgroupLayout = true;
    }

    const auto threadGroupSwizzleMode =
        pDevice->GetShaderOptimizer()->OverrideThreadGroupSwizzleMode(
            ShaderStageCompute,
            pipelineProfileKey);

    const bool threadIdSwizzleMode =
        pDevice->GetShaderOptimizer()->OverrideThreadIdSwizzleMode(
            ShaderStageCompute,
            pipelineProfileKey);

    uint32_t overrideShaderThreadGroupSizeX = 0;
    uint32_t overrideShaderThreadGroupSizeY = 0;
    uint32_t overrideShaderThreadGroupSizeZ = 0;

    pDevice->GetShaderOptimizer()->OverrideShaderThreadGroupSize(
            ShaderStageCompute,
            pipelineProfileKey,
            &overrideShaderThreadGroupSizeX,
            &overrideShaderThreadGroupSizeY,
            &overrideShaderThreadGroupSizeZ);

    if (threadGroupSwizzleMode != Vkgc::ThreadGroupSwizzleMode::Default)
    {
        pPipelineBuildInfo->options.threadGroupSwizzleMode = threadGroupSwizzleMode;
    }

    if ((overrideShaderThreadGroupSizeX == NotOverrideThreadGroupSizeX) &&
        (overrideShaderThreadGroupSizeY == NotOverrideThreadGroupSizeX) &&
        (overrideShaderThreadGroupSizeZ == NotOverrideShaderThreadGroupSizeZ) &&
        (settings.overrideThreadGroupSizeX == NotOverrideThreadGroupSizeX) &&
        (settings.overrideThreadGroupSizeY == NotOverrideThreadGroupSizeY) &&
        (settings.overrideThreadGroupSizeZ == NotOverrideThreadGroupSizeZ))
    {
        if (threadIdSwizzleMode)
        {
            pPipelineBuildInfo->options.forceCsThreadIdSwizzling = threadIdSwizzleMode;
        }
    }
    else
    {
        pPipelineBuildInfo->options.forceCsThreadIdSwizzling = settings.forceCsThreadIdSwizzling;
    }

    if(overrideShaderThreadGroupSizeX != NotOverrideThreadGroupSizeX)
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeX = overrideShaderThreadGroupSizeX;
    }
    else
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeX = settings.overrideThreadGroupSizeX;
    }

    if(overrideShaderThreadGroupSizeY != NotOverrideThreadGroupSizeY)
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeY = overrideShaderThreadGroupSizeY;
    }
    else
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeY = settings.overrideThreadGroupSizeY;
    }

    if(overrideShaderThreadGroupSizeZ != NotOverrideThreadGroupSizeZ)
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeZ = overrideShaderThreadGroupSizeZ;
    }
    else
    {
        pPipelineBuildInfo->options.overrideThreadGroupSizeZ = settings.overrideThreadGroupSizeZ;
    }

#if VKI_RAY_TRACING
    const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(pPipelineBuildInfo->cs.pModuleData);
    if (pModuleData != nullptr)
    {
        // Propagate internal shader compilation options from module to pipeline
        pPipelineBuildInfo->options.internalRtShaders = pModuleData->usage.isInternalRtShader;
        pCreateInfo->pBinaryMetadata->rayQueryUsed    = pModuleData->usage.enableRayQuery;
    }
#endif

    // By default the client hash provided to PAL is more accurate than the one used by pipeline
    // profiles.
    //
    // Optionally (based on panel setting), these can be set to temporarily match by devs.  This
    // can be useful when other tools (such as PAL's profiling layer) are used to measure shaders
    // while building a pipeline profile which uses the profile hash.
    if (settings.pipelineUseProfileHashAsClientHash)
    {
        VK_ASSERT(pipelineProfileKey.shaderCount == 1);
        VK_ASSERT(pipelineProfileKey.pShaders[0].stage == ShaderStage::ShaderStageCompute);

        pPipelineBuildInfo->cs.options.clientHash.lower = pipelineProfileKey.pShaders[0].codeHash.lower;
        pPipelineBuildInfo->cs.options.clientHash.upper = pipelineProfileKey.pShaders[0].codeHash.upper;
    }

    // Build pipline binary
    auto llpcResult = m_pLlpc->BuildComputePipeline(pPipelineBuildInfo, &pipelineOut, pPipelineDumpHandle);
    pCreateInfo->pipelineFeedback = {};
    pCreateInfo->stageFeedback = {};
    if (llpcResult != Vkgc::Result::Success)
    {
        if (llpcResult == Vkgc::Result::ErrorOutOfMemory)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    else
    {
        *ppPipelineBinary = pipelineOut.pipelineBin.pCode;
        *pPipelineBinarySize = pipelineOut.pipelineBin.codeSize;
        if (pipelineOut.pipelineCacheAccess != Llpc::CacheAccessInfo::CacheNotChecked)
        {
            pCreateInfo->pipelineFeedback.feedbackValid = true;
            pCreateInfo->pipelineFeedback.hitApplicationCache =
                (pipelineOut.pipelineCacheAccess == Llpc::CacheAccessInfo::CacheHit);
        }
        if (pipelineOut.stageCacheAccess != Llpc::CacheAccessInfo::CacheNotChecked)
        {
            pCreateInfo->stageFeedback.feedbackValid = true;
            pCreateInfo->stageFeedback.hitApplicationCache =
                (pipelineOut.stageCacheAccess == Llpc::CacheAccessInfo::CacheHit);
        }
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            VK_ASSERT(pipelineProfileKey.shaderCount == 1);
            VK_ASSERT(pipelineProfileKey.pShaders[0].stage == ShaderStage::ShaderStageCompute);

            char                      extraInfo[256] = {};
            const ShaderOptimizerKey& shaderKey      = pipelineProfileKey.pShaders[0];

            Util::Snprintf(extraInfo, sizeof(extraInfo), "\n\n;PipelineOptimizer\n");
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);

            if (shaderKey.codeHash.upper || shaderKey.codeHash.lower)
            {
                const char* pName = GetShaderStageName(ShaderStage::ShaderStageCompute);
                Util::Snprintf(
                    extraInfo,
                    sizeof(extraInfo),
                    ";%s Shader Profile Key: 0x%016" PRIX64 "%016" PRIX64 ",\n",
                    pName,
                    shaderKey.codeHash.upper,
                    shaderKey.codeHash.lower);
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
            }
        }
    }

    *pCompileTime = Util::GetPerfCpuTime() - startTime;

    return result;
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void HelperThreadBuildRayTracingElf(
    void* pPayload)
{
    LlpcHelperThreadProvider::HelperThreadProviderPayload* pHelperPayload =
        static_cast<LlpcHelperThreadProvider::HelperThreadProviderPayload*>(pPayload);

    pHelperPayload->pFunction(pHelperPayload->pHelperProvider, pHelperPayload->pPayload);
}

// =====================================================================================================================
void LlpcHelperThreadProvider::SetTasks(
    ThreadFunction* pFunction,
    uint32_t        numTasks,
    void*           pPayload)
{
    m_pDeferredWorkload->Execute = &HelperThreadBuildRayTracingElf;
    m_payload = { this, pFunction, pPayload };
    m_pDeferredWorkload->pPayloads = &m_payload;
    Util::AtomicExchange(&m_pDeferredWorkload->totalInstances, numTasks);
}

// =====================================================================================================================
bool LlpcHelperThreadProvider::GetNextTask(
    uint32_t* pTaskIndex)
{
    VK_ASSERT(pTaskIndex != nullptr);
    *pTaskIndex = Util::AtomicIncrement(&m_pDeferredWorkload->nextInstance) - 1;
    return (*pTaskIndex < m_pDeferredWorkload->totalInstances);
}

// =====================================================================================================================
void LlpcHelperThreadProvider::TaskCompleted()
{
    uint32_t completedInstances = Util::AtomicIncrement(&m_pDeferredWorkload->completedInstances);
    if (completedInstances == m_pDeferredWorkload->totalInstances)
    {
        m_pDeferredWorkload->event.Set();
    }
}

// =====================================================================================================================
void LlpcHelperThreadProvider::WaitForTasks()
{
    while (m_pDeferredWorkload->completedInstances < m_pDeferredWorkload->totalInstances)
    {
        m_pDeferredWorkload->event.Wait(1.0f);
    }
}

// =====================================================================================================================
// Creates ray tracing pipeline binary.
VkResult CompilerSolutionLlpc::CreateRayTracingPipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    RayTracingPipelineBinaryCreateInfo* pCreateInfo,
    RayTracingPipelineBinary*           pPipelineBinary,
    void*                               pPipelineDumpHandle,
    uint64_t                            pipelineHash,
    Util::MetroHash::Hash*              pCacheId,
    int64_t*                            pCompileTime)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result = VK_SUCCESS;

    LlpcHelperThreadProvider llpcHelperThreadProvider(pCreateInfo->pDeferredWorkload);
    LlpcHelperThreadProvider* pLlpcHelperThreadProvider = nullptr;
    if (pCreateInfo->pDeferredWorkload != nullptr)
    {
        pLlpcHelperThreadProvider = &llpcHelperThreadProvider;
    }

    // Build the LLPC pipeline
    Llpc::RayTracingPipelineBuildOut pipelineOut = {};

    int64_t startTime = Util::GetPerfCpuTime();
    // Fill pipeline create info for LLPC
    auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
    pPipelineBuildInfo->pInstance = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
    pPipelineBuildInfo->deviceIndex = deviceIdx;
    pPipelineBuildInfo->deviceCount = m_pPhysicalDevice->Manager()->GetDeviceCount();

    auto llpcResult = m_pLlpc->BuildRayTracingPipeline(pPipelineBuildInfo,
                                                       &pipelineOut,
                                                       pPipelineDumpHandle,
                                                       pLlpcHelperThreadProvider);
    if (llpcResult != Vkgc::Result::Success)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }
    else
    {
        pPipelineBinary->hasTraceRay = pipelineOut.hasTraceRay;

        const uint32_t shaderGroupHandleSize =
            sizeof(Vkgc::RayTracingShaderIdentifier) * pipelineOut.shaderGroupHandle.shaderHandleCount;

        for (unsigned i = 0; i < pipelineOut.pipelineBinCount; ++i)
        {
            pPipelineBinary->pPipelineBins[i].pCode = pipelineOut.pipelineBins[i].pCode;
            pPipelineBinary->pPipelineBins[i].codeSize = pipelineOut.pipelineBins[i].codeSize;
        }
        pPipelineBinary->pipelineBinCount = pipelineOut.pipelineBinCount;

        auto shaderPropSetSize = pipelineOut.shaderPropSet.shaderCount * sizeof(Vkgc::RayTracingShaderProperty);
        if (shaderPropSetSize > 0)
        {
            Vkgc::RayTracingShaderProperty* pShaderProp = pPipelineBinary->shaderPropSet.shaderProps;
            memcpy(pShaderProp, pipelineOut.shaderPropSet.shaderProps, shaderPropSetSize);
        }
        pPipelineBinary->shaderPropSet.shaderCount = pipelineOut.shaderPropSet.shaderCount;
        pPipelineBinary->shaderPropSet.traceRayIndex = pipelineOut.shaderPropSet.traceRayIndex;

        memcpy(pPipelineBinary->shaderGroupHandle.shaderHandles, pipelineOut.shaderGroupHandle.shaderHandles, shaderGroupHandleSize);
        pPipelineBinary->shaderGroupHandle.shaderHandleCount = pipelineOut.shaderGroupHandle.shaderHandleCount;
        void* pOutShaderGroup = static_cast<void*>(pipelineOut.shaderGroupHandle.shaderHandles);
    }
    *pCompileTime = Util::GetPerfCpuTime() - startTime;

    return result;
}

// =====================================================================================================================
void CompilerSolutionLlpc::FreeRayTracingPipelineBinary(
    RayTracingPipelineBinary* pPipelineBinary)
{
    // All pipeline binaries, including the pPipelineBins list, are packed into a single allocation that starts at the
    // first pipeline.
    if (pPipelineBinary->pipelineBinCount > 0)
    {
        auto pBin = &pPipelineBinary->pPipelineBins[0];
        if (pBin->pCode != nullptr)
        {
            m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pBin->pCode));
        }
    }
}
#endif

// =====================================================================================================================
void CompilerSolutionLlpc::FreeGraphicsPipelineBinary(
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
}

// =====================================================================================================================
void CompilerSolutionLlpc::FreeComputePipelineBinary(
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
}

// =====================================================================================================================
// Create the LLPC compiler
VkResult CompilerSolutionLlpc::CreateLlpcCompiler(
    Vkgc::ICache* pCache)
{
    const uint32_t         OptionBufferSize = 4096;
    const uint32_t         MaxLlpcOptions   = 32;
    Llpc::ICompiler*       pCompiler        = nullptr;
    const RuntimeSettings& settings         = m_pPhysicalDevice->GetRuntimeSettings();
    AppProfile             appProfile       = m_pPhysicalDevice->GetAppProfile();
    // Get the executable name and path
    char  executableNameBuffer[PATH_MAX];
    char* pExecutablePtr;
    Pal::Result palResult = Util::GetExecutableName(&executableNameBuffer[0],
                                                    &pExecutablePtr,
                                                    sizeof(executableNameBuffer));
    VK_ASSERT(palResult == Pal::Result::Success);

    // Initialize LLPC options according to runtime settings
    const char*        llpcOptions[MaxLlpcOptions]     = {};
    char               optionBuffers[OptionBufferSize] = {};

    char*              pOptionBuffer                   = &optionBuffers[0];
    size_t             bufSize                         = OptionBufferSize;
    int                optionLength                    = 0;
    uint32_t           numOptions                      = 0;
    // Identify for Icd and stanalone compiler
    llpcOptions[numOptions++] = Llpc::VkIcdName;

    // LLPC log options
    llpcOptions[numOptions++] = (settings.enableLog & 1) ? "-enable-errs=1" : "-enable-errs=0";
    llpcOptions[numOptions++] = (settings.enableLog & 2) ? "-enable-outs=1" : "-enable-outs=0";

    char logFileName[PATH_MAX] = {};

    Util::Snprintf(&logFileName[0], PATH_MAX, "%s/%sLlpc", settings.pipelineDumpDir, settings.logFileName);

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-log-file-outs=%s", logFileName);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-log-file-dbgs=%s", settings.debugLogFileName);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    // LLPC debug options
    if (settings.enableDebug)
    {
        llpcOptions[numOptions++] = "-debug";
    }

    // LLPC pipeline dump options
    if (settings.enablePipelineDump)
    {
        llpcOptions[numOptions++] = "-enable-pipeline-dump";
    }

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-pipeline-dump-dir=%s", settings.pipelineDumpDir);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    // NOTE: For testing consistency, these options should be kept the same as those of
    // "amdllpc" (Init()).
    // WARNING: Do not conditionally add options based on GFXIP version as these will
    // break support for systems with a mixture of ASICs. GFXIP dependent options
    // should be subtarget features or handled in LLVM backend.

    if ((appProfile == AppProfile::SeriousSamFusion) ||
        (appProfile == AppProfile::Talos))
    {
        llpcOptions[numOptions++] = "-unroll-partial-threshold=700";
    }

    ShaderCacheMode shaderCacheMode = settings.shaderCacheMode;
    if ((appProfile == AppProfile::MadMax) ||
        (appProfile == AppProfile::SedpEngine) ||
        (appProfile == AppProfile::ThronesOfBritannia))
    {
        llpcOptions[numOptions++] = "-enable-si-scheduler";
        // si-scheduler interacts badly with SIFormMemoryClauses pass, so
        // disable the effect of that pass by limiting clause length to 1.
        llpcOptions[numOptions++] = "-amdgpu-max-memory-clause=1";
    }

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 66
    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-executable-name=%s", pExecutablePtr);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-cache-file-dir=%s",
        m_pPhysicalDevice->PalDevice()->GetCacheFilePath());
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-cache-mode=%d", shaderCacheMode);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;
#endif

    llpcOptions[numOptions++] = "-cache-full-pipelines=0";

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-subgroup-size=%d", m_pPhysicalDevice->GetSubgroupSize());
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    if (settings.llpcOptions[0] != '\0')
    {
        const char* pOptions = &settings.llpcOptions[0];
        VK_ASSERT(pOptions[0] == '-');

        // Split options
        while (pOptions)
        {
            const char* pNext = strchr(pOptions, ' ');
            const char* pOption = nullptr;
            if (pNext)
            {
                // Copy options to option buffer
                optionLength = static_cast<int32_t>(pNext - pOptions);
                memcpy(pOptionBuffer, pOptions, optionLength);
                pOptionBuffer[optionLength] = 0;
                pOption = pOptionBuffer;
                pOptionBuffer += (optionLength + 1);
                bufSize -= (optionLength + 1);
                pOptions = strchr(pOptions + optionLength, '-');
            }
            else
            {
                pOption = pOptions;
                pOptions = nullptr;
            }

            const char* pNameEnd = strchr(pOption, '=');
            size_t nameLength = (pNameEnd != nullptr) ? (pNameEnd - pOption) : strlen(pOption);
            uint32_t optionIndex = UINT32_MAX;
            for (uint32_t i = 0; i < numOptions; ++i)
            {
                if (strncmp(llpcOptions[i], pOption, nameLength) == 0)
                {
                    optionIndex = i;
                    break;
                }
            }

            if (optionIndex != UINT32_MAX)
            {
                llpcOptions[optionIndex] = pOption;
            }
            else
            {
                llpcOptions[numOptions++] = pOption;
            }
        }
    }

    VK_ASSERT(numOptions <= MaxLlpcOptions);

    // Create LLPC compiler
    Vkgc::Result llpcResult = Llpc::ICompiler::Create(m_gfxIp, numOptions, llpcOptions, &pCompiler, pCache);
    VK_ASSERT(llpcResult == Vkgc::Result::Success);

    m_pLlpc = pCompiler;

    return (llpcResult == Vkgc::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Write internal data for pipeline
void CompilerSolutionLlpc::BuildPipelineInternalBufferData(
    const PipelineCompiler*           pCompiler,
    const uint32_t                    uberFetchConstBufRegBase,
    const uint32_t                    specConstBufVertexRegBase,
    const uint32_t                    specConstBufFragmentRegBase,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    auto                  pInstance = m_pPhysicalDevice->VkInstance();
    uint32_t internalBufferSize = 0;

    // NOTE: Using instance divisor may get an incorrect result, disabled it on LLPC.
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    VK_ASSERT(settings.disableInstanceDivisorOpt);

    const VkPipelineVertexInputStateCreateInfo* pVertexInput = nullptr;
    if (pCreateInfo->pipelineInfo.enableUberFetchShader || pCreateInfo->pipelineInfo.enableEarlyCompile)
    {
        pVertexInput = pCreateInfo->pipelineInfo.pVertexInput;
        internalBufferSize += pCompiler->GetUberFetchShaderInternalDataSize(pVertexInput);
    }
    auto pInternalBufferInfo = &(pCreateInfo->pBinaryMetadata->internalBufferInfo);
    pInternalBufferInfo->dataSize = internalBufferSize;

    if (internalBufferSize > 0)
    {
        pInternalBufferInfo->pData = pInstance->AllocMem(internalBufferSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        if (pInternalBufferInfo->pData == nullptr)
        {
            pCreateInfo->pipelineInfo.enableEarlyCompile = false;
            pCreateInfo->pipelineInfo.enableUberFetchShader = false;
        }
    }

    uint32_t internalBufferOffset = 0;
    if ((pInternalBufferInfo->pData) &&
        (pVertexInput != nullptr) &&
        (pVertexInput->vertexAttributeDescriptionCount > 0))
    {
        uint32_t uberFetchShaderInternalDataSize = pCompiler->BuildUberFetchShaderInternalData(
            pVertexInput, pCreateInfo->pipelineInfo.dynamicVertexStride, pInternalBufferInfo->pData);

        if (uberFetchShaderInternalDataSize == 0)
        {
            pCreateInfo->pipelineInfo.enableUberFetchShader = false;
        }
        else
        {
            auto pBufferEntry = &pInternalBufferInfo->internalBufferEntries[pInternalBufferInfo->internalBufferCount++];
            pBufferEntry->userDataOffset = uberFetchConstBufRegBase;
            pBufferEntry->bufferOffset = internalBufferOffset;
            internalBufferOffset += uberFetchShaderInternalDataSize;
        }
    }
}

// =====================================================================================================================
// Update the cache feedback for a stage.
void CompilerSolutionLlpc::UpdateStageCreationFeedback(
    PipelineCreationFeedback*       pStageFeedback,
    const Vkgc::PipelineShaderInfo& shader,
    const Llpc::CacheAccessInfo*    pStageCacheAccesses,
    ShaderStage                     stage)
{
    if ((shader.pModuleData != nullptr) && (pStageCacheAccesses[stage] != Llpc::CacheAccessInfo::CacheNotChecked))
    {
        pStageFeedback[stage].feedbackValid = true;
        pStageFeedback[stage].hitApplicationCache = (pStageCacheAccesses[stage] == Llpc::CacheAccessInfo::CacheHit);
    }
}
}
