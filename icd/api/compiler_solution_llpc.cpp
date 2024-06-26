/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace std::chrono_literals;

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

    PipelineCompiler::DumpCacheMatrix(m_pPhysicalDevice, "GraphicsPipelineLibrary", UINT32_MAX, &m_gplCacheMatrix);
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult CompilerSolutionLlpc::BuildShaderModule(
    const Device*                pDevice,
    VkShaderModuleCreateFlags    flags,
    VkShaderModuleCreateFlags    internalShaderFlags,
    const Vkgc::BinaryData&      shaderBinary,
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
    moduleInfo.shaderBin      = shaderBinary;

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
    pShaderModule->pLlpcShaderModule = nullptr;
}

// =====================================================================================================================
// Creates graphics pipeline binary.
VkResult CompilerSolutionLlpc::CreateGraphicsPipelineBinary(
    const Device*                     pDevice,
    uint32_t                          deviceIdx,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    Vkgc::BinaryData*                 pPipelineBinary,
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
            elfPackage[UnlinkedStageVertexProcess] =
                ExtractPalElfBinary(pCreateInfo->earlyElfPackage[GraphicsLibraryPreRaster]);
        }

        if (pCreateInfo->earlyElfPackage[GraphicsLibraryFragment].pCode != nullptr)
        {
            elfPackage[UnlinkedStageFragment] =
                ExtractPalElfBinary(pCreateInfo->earlyElfPackage[GraphicsLibraryFragment]);
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
        *pPipelineBinary = pipelineOut.pipelineBin;
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
    pPipelineBuildInfo->pipelineApiHash     = pCreateInfo->libraryHash[GraphicsLibraryColorExport];

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
    GraphicsLibraryType               gplType,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    void*                             pPipelineDumpHandle,
    GplModuleState*                   pModuleState)
{
    VkResult result = VK_SUCCESS;
    Util::MetroHash::Hash cacheId = {};
    Vkgc::BinaryData shaderLibraryBinary = {};

    bool hitCache = false;
    bool hitAppCache = false;
    bool elfReplace = false;
    if (pCreateInfo->earlyElfPackage[gplType].pCode == nullptr)
    {
        Util::MetroHash128              hasher;
        Llpc::GraphicsPipelineBuildOut  pipelineOut = {};
        int64_t                         startTime = Util::GetPerfCpuTime();

        hasher.Update(pCreateInfo->libraryHash[gplType]);
        hasher.Update(PipelineCompilerTypeLlpc);
        hasher.Update(m_pPhysicalDevice->GetSettingsLoader()->GetSettingsHash());
        hasher.Finalize(cacheId.bytes);

        Vkgc::BinaryData finalBinary = {};
        if ((pDevice->GetRuntimeSettings().shaderReplaceMode == ShaderReplacePipelineBinaryHash) ||
            (pDevice->GetRuntimeSettings().shaderReplaceMode == ShaderReplaceShaderHashPipelineBinaryHash))
        {
            elfReplace = PipelineCompiler::ReplacePipelineBinary(pDevice->VkPhysicalDevice(DefaultDeviceIndex),
                                                                 &pCreateInfo->pipelineInfo,
                                                                 &finalBinary,
                                                                 pCreateInfo->libraryHash[gplType]);
            // Force hit flags to avoid update cache
            if (elfReplace)
            {
                hitCache = true;
                hitAppCache = true;
            }
        }

        if (elfReplace == false)
        {
            const uint32_t gplStageMask = (gplType == GraphicsLibraryPreRaster) ?
                    PrsShaderMask : FgsShaderMask;
            const Vkgc::PipelineShaderInfo* pShadersInfo[ShaderStage::ShaderStageGfxCount] =
            {
                &pCreateInfo->pipelineInfo.task,
                &pCreateInfo->pipelineInfo.vs,
                &pCreateInfo->pipelineInfo.tcs,
                &pCreateInfo->pipelineInfo.tes,
                &pCreateInfo->pipelineInfo.gs,
                &pCreateInfo->pipelineInfo.mesh,
                &pCreateInfo->pipelineInfo.fs
            };
            LoadShaderBinaryFromCache(pPipelineCache, &cacheId, &shaderLibraryBinary, &hitCache, &hitAppCache);
            if (pPipelineCache != nullptr)
            {
                // Update the shader feedback
                for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; stage++)
                {
                    if (Util::TestAnyFlagSet(gplStageMask, 1 << stage) &&
                        ((pShadersInfo[stage]->options.clientHash.lower != 0) ||
                         (pShadersInfo[stage]->options.clientHash.upper != 0)))
                    {
                        PipelineCreationFeedback* pStageFeedBack = &pCreateInfo->stageFeedback[stage];
                        pStageFeedBack->feedbackValid = true;
                        pStageFeedBack->hitApplicationCache = hitAppCache;
                    }
                }
            }

            bool checkShaderModuleIdUsage = false;
            if (hitCache)
            {
                const auto* pShaderLibraryHeader =
                    reinterpret_cast<const ShaderLibraryBlobHeader*>(shaderLibraryBinary.pCode);
                if (pShaderLibraryHeader->requireFullPipeline)
                {
                    checkShaderModuleIdUsage = true;
                }
            }
            else
            {
                checkShaderModuleIdUsage = true;
            }

            if (checkShaderModuleIdUsage)
            {
                for (uint32_t stage = 0; stage < ShaderStage::ShaderStageGfxCount; stage++)
                {
                    if (Util::TestAnyFlagSet(gplStageMask, 1 << stage) &&
                        (pShadersInfo[stage]->pModuleData == nullptr) &&
                        ((pShadersInfo[stage]->options.clientHash.lower != 0) ||
                         (pShadersInfo[stage]->options.clientHash.upper != 0)))
                    {
                        result = VK_PIPELINE_COMPILE_REQUIRED_EXT;
                        break;
                    }
                }
            }
        }

        ShaderLibraryBlobHeader blobHeader = {};
        if ((result == VK_SUCCESS) && ((hitCache == false) || elfReplace))
        {
            // Build the LLPC pipeline
            Vkgc::UnlinkedShaderStage unlinkedStage = UnlinkedStageCount;

            // Belong to vertexProcess stage before fragment
            if (gplType == GraphicsLibraryPreRaster)
            {
                unlinkedStage = UnlinkedShaderStage::UnlinkedStageVertexProcess;
            }
            else if (gplType == GraphicsLibraryFragment)
            {
                unlinkedStage = UnlinkedShaderStage::UnlinkedStageFragment;
            }

            auto llpcResult = m_pLlpc->buildGraphicsShaderStage(
                &pCreateInfo->pipelineInfo,
                &pipelineOut,
                unlinkedStage,
                pPipelineDumpHandle);

            if (elfReplace == false)
            {
                finalBinary = pipelineOut.pipelineBin;
            }

            if (llpcResult == Vkgc::Result::Success)
            {
                blobHeader.binaryLength = finalBinary.codeSize;
                blobHeader.fragMetaLength = pipelineOut.fsOutputMetaDataSize;
            }
            else if (llpcResult == Vkgc::Result::RequireFullPipeline)
            {
                blobHeader.requireFullPipeline = true;
            }
            else
            {
                result = (llpcResult == Vkgc::Result::ErrorOutOfMemory) ? VK_ERROR_OUT_OF_HOST_MEMORY :
                                                                          VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        if (result == VK_SUCCESS)
        {
            // Always call StoreShaderBinaryToCache to sync data between app cache and binary cache except
            // RequireFullPipeline. When cache is hit, blobHeader is zero, StoreShaderBinaryToCache will ignore
            // finalBinary, and reuse shaderLibraryBinary.
            StoreShaderBinaryToCache(
                pPipelineCache,
                &cacheId,
                &blobHeader,
                finalBinary.pCode,
                pipelineOut.fsOutputMetaData,
                hitCache,
                hitAppCache,
                &shaderLibraryBinary);

            pModuleState->elfPackage                  = shaderLibraryBinary;
            pModuleState->pFsOutputMetaData           = nullptr;
            pCreateInfo->earlyElfPackage[gplType]     = pModuleState->elfPackage;
            pCreateInfo->earlyElfPackageHash[gplType] = cacheId;

            if (gplType == GraphicsLibraryFragment)
            {
                if (shaderLibraryBinary.pCode != nullptr)
                {
                    const auto* pShaderLibraryHeader =
                        reinterpret_cast<const ShaderLibraryBlobHeader*>(shaderLibraryBinary.pCode);

                    pCreateInfo->pBinaryMetadata->fsOutputMetaDataSize = 0;
                    pCreateInfo->pBinaryMetadata->pFsOutputMetaData    = nullptr;
                    if (pShaderLibraryHeader->fragMetaLength > 0)
                    {
                        void* pFsOutputMetaData = m_pPhysicalDevice->Manager()->VkInstance()->AllocMem(
                            pShaderLibraryHeader->fragMetaLength,
                            VK_DEFAULT_MEM_ALIGN,
                            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

                        if (pFsOutputMetaData != nullptr)
                        {
                            memcpy(pFsOutputMetaData,
                                VoidPtrInc(pShaderLibraryHeader + 1, pShaderLibraryHeader->binaryLength),
                                pShaderLibraryHeader->fragMetaLength);
                            pCreateInfo->pBinaryMetadata->fsOutputMetaDataSize = pShaderLibraryHeader->fragMetaLength;
                            pCreateInfo->pBinaryMetadata->pFsOutputMetaData    = pFsOutputMetaData;
                            pModuleState->pFsOutputMetaData                    = pFsOutputMetaData;
                        }
                        else
                        {
                            result = VK_ERROR_OUT_OF_HOST_MEMORY;
                        }
                    }
                }
            }

            const PipelineShaderInfo* pShaders[ShaderStage::ShaderStageGfxCount] =
            {
                &pCreateInfo->pipelineInfo.task,
                &pCreateInfo->pipelineInfo.vs,
                &pCreateInfo->pipelineInfo.tcs,
                &pCreateInfo->pipelineInfo.tes,
                &pCreateInfo->pipelineInfo.gs,
                &pCreateInfo->pipelineInfo.mesh,
                &pCreateInfo->pipelineInfo.fs,
            };

            for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; ++i)
            {
                if ((pShaders[i]->pModuleData != nullptr) &&
                    (GetGraphicsLibraryType(static_cast<ShaderStage>(i)) == gplType))
                {
                    const auto* pModuleData =
                        reinterpret_cast<const Vkgc::ShaderModuleData*>(pShaders[i]->pModuleData);
#if VKI_RAY_TRACING
                    pCreateInfo->pBinaryMetadata->rayQueryUsed            |= pModuleData->usage.enableRayQuery;
#endif
                    pCreateInfo->pBinaryMetadata->pointSizeUsed           |= pModuleData->usage.usePointSize;
                    pCreateInfo->pBinaryMetadata->shadingRateUsedInShader |= pModuleData->usage.useShadingRate;
                }
            }

            if (elfReplace && (finalBinary.pCode != nullptr))
            {
                m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(finalBinary.pCode));
            }

            if ((pPipelineDumpHandle != nullptr) && (pCreateInfo->earlyElfPackage[gplType].pCode != nullptr))
            {
                if (pCreateInfo->earlyElfPackage[gplType].pCode != nullptr)
                {
                    Vkgc::BinaryData elfBinary = ExtractPalElfBinary(pCreateInfo->earlyElfPackage[gplType]);
                    Vkgc::IPipelineDumper::DumpPipelineBinary(
                        pPipelineDumpHandle, m_gfxIp, &elfBinary);
                }
            }
        }

        // PipelineBin has been translated to blob start with ShaderLibraryBlobHeader in StoreShaderBinaryToCache
        if (pipelineOut.pipelineBin.pCode != nullptr)
        {
            FreeGraphicsPipelineBinary(pipelineOut.pipelineBin);
        }

        m_gplCacheMatrix.totalBinaries++;
        m_gplCacheMatrix.totalTimeSpent += (Util::GetPerfCpuTime() - startTime);

        PipelineCompiler::DumpCacheMatrix(
            m_pPhysicalDevice, "GraphicsPipelineLibrary_runtime", m_gplCacheMatrix.totalBinaries, &m_gplCacheMatrix);

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
    Vkgc::BinaryData*                pPipelineBinary,
    void*                            pPipelineDumpHandle,
    uint64_t                         pipelineHash,
    Util::MetroHash::Hash*           pCacheId,
    int64_t*                         pCompileTime)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    Vkgc::ComputePipelineBuildInfo* pPipelineBuildInfo = &pCreateInfo->pipelineInfo;

    VkResult result = VK_SUCCESS;

    int64_t startTime = Util::GetPerfCpuTime();

    const auto& pipelineProfileKey = *pCreateInfo->pPipelineProfileKey;

    // Build the LLPC pipeline
    Llpc::ComputePipelineBuildOut pipelineOut = {};

    // Fill pipeline create info for LLPC
    pPipelineBuildInfo->pInstance      = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;

#if VKI_RAY_TRACING
    const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(pPipelineBuildInfo->cs.pModuleData);
    if (pModuleData != nullptr)
    {
        // Propagate internal shader compilation options from module to pipeline
        pPipelineBuildInfo->options.internalRtShaders = pModuleData->usage.isInternalRtShader;
        pCreateInfo->pBinaryMetadata->rayQueryUsed    = pModuleData->usage.enableRayQuery;
    }
#endif

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
        *pPipelineBinary = pipelineOut.pipelineBin;
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
        m_pDeferredWorkload->event.Wait(1s);
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

        pPipelineBinary->librarySummary = pipelineOut.librarySummary;
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
    const Vkgc::BinaryData& pipelineBinary)
{
    m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pipelineBinary.pCode));
}

// =====================================================================================================================
void CompilerSolutionLlpc::FreeComputePipelineBinary(
    const Vkgc::BinaryData& pipelineBinary)
{
    m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pipelineBinary.pCode));
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

    if (settings.enableImageMsaaLoadOpt)
    {
        llpcOptions[numOptions++] = "-mattr=-msaa-load-dst-sel-bug";
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
    bool                              needCache,
    GraphicsPipelineBinaryCreateInfo* pCreateInfo)
{
    auto                  pInstance           = m_pPhysicalDevice->VkInstance();
    auto                  pInternalBufferInfo = &(pCreateInfo->pBinaryMetadata->internalBufferInfo);

    pInternalBufferInfo->dataSize = 0;
    // NOTE: Using instance divisor may get an incorrect result, disabled it on LLPC.
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    VK_ASSERT(settings.disableInstanceDivisorOpt);

    const VkPipelineVertexInputStateCreateInfo* pVertexInput = nullptr;
    bool needUberFetchShaderBuffer = false;

    if (pCreateInfo->pipelineInfo.enableUberFetchShader || pCreateInfo->pipelineInfo.enableEarlyCompile)
    {
        pVertexInput = pCreateInfo->pipelineInfo.pVertexInput;
        // For monolithic pipeline (needCache = true), we need save internal buffer data to cache, so we always need
        // build full internal buffer. But in GPL fast link mode or vertex input library, we needn't add it to cache,
        // so if we have copied uber-fetch shader buffer from input library.i.e internalBufferCount is not zero, we
        // needn't build internal buffer for uber-fetch shader again.
        if (needCache || (pInternalBufferInfo->internalBufferCount == 0))
        {
            uint32_t uberFetchShaderInternalDataSize = pCompiler->GetUberFetchShaderInternalDataSize(pVertexInput);
            if (uberFetchShaderInternalDataSize > 0)
            {
                needUberFetchShaderBuffer = true;
                pInternalBufferInfo->dataSize += uberFetchShaderInternalDataSize;
            }
        }
    }

    if (pInternalBufferInfo->dataSize > 0)
    {
        pInternalBufferInfo->pData = pInstance->AllocMem(pInternalBufferInfo->dataSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        if (pInternalBufferInfo->pData == nullptr)
        {
            pCreateInfo->pipelineInfo.enableEarlyCompile = false;
            pCreateInfo->pipelineInfo.enableUberFetchShader = false;
            pInternalBufferInfo->dataSize = 0;
            needUberFetchShaderBuffer = false;
        }
    }

    uint32_t internalBufferOffset = 0;
    if (needUberFetchShaderBuffer)
    {
        uint32_t uberFetchShaderInternalDataSize = pCompiler->BuildUberFetchShaderInternalData(
            pVertexInput, pCreateInfo->pipelineInfo.dynamicVertexStride,
            pCreateInfo->pipelineInfo.useSoftwareVertexBufferDescriptors, pInternalBufferInfo->pData);

        auto pBufferEntry = &pInternalBufferInfo->internalBufferEntries[0];
        pBufferEntry->userDataOffset = uberFetchConstBufRegBase;
        pBufferEntry->bufferOffset = internalBufferOffset;
        internalBufferOffset += uberFetchShaderInternalDataSize;
        pInternalBufferInfo->internalBufferCount = 1;

    }
    else if (pInternalBufferInfo->internalBufferCount > 0)
    {
        auto pBufferEntry = &pInternalBufferInfo->internalBufferEntries[0];
        VK_ASSERT(pBufferEntry->bufferAddress[DefaultDeviceIndex] != 0);
        pBufferEntry->userDataOffset = uberFetchConstBufRegBase;
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

// =====================================================================================================================
bool CompilerSolutionLlpc::IsGplFastLinkCompatible(
    const Device*                           pDevice,
    uint32_t                                deviceIdx,
    const GraphicsPipelineBinaryCreateInfo* pCreateInfo,
    const GraphicsPipelineLibraryInfo&      libInfo)
{
    return (pCreateInfo->pipelineInfo.iaState.enableMultiView == false) &&
           ((pCreateInfo->flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) == 0);
}

// =====================================================================================================================
Vkgc::BinaryData CompilerSolutionLlpc::ExtractPalElfBinary(
    const Vkgc::BinaryData& shaderBinary)
{
    Vkgc::BinaryData elfBinary = {};
    const ShaderLibraryBlobHeader* pHeader = reinterpret_cast<const ShaderLibraryBlobHeader*>(shaderBinary.pCode);
    if (pHeader->binaryLength > 0)
    {
        elfBinary.pCode = pHeader + 1;
        elfBinary.codeSize = pHeader->binaryLength;
    }
    return elfBinary;
}

}
