/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <inttypes.h>

namespace vk
{

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
    Vkgc::GfxIpVersion gfxIp,
    Pal::GfxIpLevel    gfxIpLevel,
    Vkgc::ICache*      pCache)
{
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    Vkgc::ICache* pInternalCache = pCache;
    if (settings.shaderCacheMode == ShaderCacheDisable)
    {
        pInternalCache = nullptr;
    }

    VkResult result = CompilerSolution::Initialize(gfxIp, gfxIpLevel, pInternalCache);

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
// Get size of shader cache object
size_t CompilerSolutionLlpc::GetShaderCacheSize(
    PipelineCompilerType cacheType)
{
    VK_NEVER_CALLED();
    return 0;
}

// =====================================================================================================================
// Creates shader cache object.
VkResult CompilerSolutionLlpc::CreateShaderCache(
    const void*  pInitialData,
    size_t       initialDataSize,
    void*        pShaderCacheMem,
    ShaderCache* pShaderCache)
{
    VK_IGNORE(pShaderCacheMem);

    return VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult CompilerSolutionLlpc::BuildShaderModule(
    const Device*                pDevice,
    VkShaderModuleCreateFlags    flags,
    size_t                       codeSize,
    const void*                  pCode,
    ShaderModuleHandle*          pShaderModule,
    const Util::MetroHash::Hash& hash)
{
    VK_IGNORE(pDevice);
    VK_IGNORE(hash);
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
    pPipelineCompiler->ApplyPipelineOptions(pDevice, 0, &moduleInfo.options.pipelineOptions);
    moduleInfo.options.enableOpt = (flags & VK_SHADER_MODULE_ENABLE_OPT_BIT) ? true : false;

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
// Creates partial pipeline binary for compute shader and fragment shader
VkResult CompilerSolutionLlpc::CreatePartialPipelineBinary(
    uint32_t                             deviceIdx,
    void*                                pShaderModuleData,
    Vkgc::ShaderModuleEntryData*         pShaderModuleEntryData,
    const Vkgc::ResourceMappingRootNode* pResourceMappingNode,
    uint32_t                             mappingNodeCount,
    Vkgc::ColorTarget*                   pColorTarget)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result = VK_SUCCESS;

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
    Vkgc::ResourceMappingNode* pRawNodes =
        static_cast<Vkgc::ResourceMappingNode*>(pInstance->AllocMem(mappingNodeCount * sizeof(Vkgc::ResourceMappingNodeType),
                                                VK_DEFAULT_MEM_ALIGN,
                                                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND));

    if (pRawNodes != nullptr)
    {
        for (uint32_t i = 0; i < mappingNodeCount; ++i)
        {
            pRawNodes[i] = pResourceMappingNode[i].node;
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
#endif

    if (result == VK_SUCCESS)
    {
        if (pShaderModuleEntryData->stage == Vkgc::ShaderStageCompute)
        {
            Vkgc::ComputePipelineBuildInfo pipelineBuildInfo = {};
            Llpc::ComputePipelineBuildOut  pipelineOut = {};
            void* pLlpcPipelineBuffer = nullptr;

            // Fill pipeline create info for LLPC
            pipelineBuildInfo.pInstance = pInstance;
            pipelineBuildInfo.pfnOutputAlloc = AllocateShaderOutput;
            pipelineBuildInfo.pUserData = &pLlpcPipelineBuffer;
            pipelineBuildInfo.deviceIndex = deviceIdx;
            pipelineBuildInfo.cs.pModuleData = pShaderModuleData;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
            pipelineBuildInfo.cs.pUserDataNodes = pRawNodes;
            pipelineBuildInfo.cs.userDataNodeCount = mappingNodeCount;
#else
            pipelineBuildInfo.resourceMapping.pUserDataNodes = pResourceMappingNode;
            pipelineBuildInfo.resourceMapping.userDataNodeCount = mappingNodeCount;
#endif

            auto llpcResult = m_pLlpc->BuildComputePipeline(&pipelineBuildInfo, &pipelineOut, nullptr);
            if (llpcResult != Vkgc::Result::Success)
            {
                // There shouldn't be anything to free for the failure case
                VK_ASSERT(pLlpcPipelineBuffer == nullptr);
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
            pInstance->FreeMem(const_cast<void*>(pipelineOut.pipelineBin.pCode));
        }
        else
        {
            Vkgc::GraphicsPipelineBuildInfo pipelineBuildInfo = {};
            // Build the LLPC pipeline
            Llpc::GraphicsPipelineBuildOut  pipelineOut = {};
            void* pLlpcPipelineBuffer = nullptr;

            // Fill pipeline create info for LLPC
            pipelineBuildInfo.pInstance = pInstance;
            pipelineBuildInfo.pfnOutputAlloc = AllocateShaderOutput;
            pipelineBuildInfo.pUserData = &pLlpcPipelineBuffer;
            pipelineBuildInfo.iaState.deviceIndex = deviceIdx;
            pipelineBuildInfo.fs.pModuleData = pShaderModuleData;

            pipelineBuildInfo.fs.pEntryTarget = pShaderModuleEntryData->pEntryName;
            pipelineBuildInfo.fs.entryStage = pShaderModuleEntryData->stage;
#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
            pipelineBuildInfo.fs.pUserDataNodes = pRawNodes;
            pipelineBuildInfo.fs.userDataNodeCount = mappingNodeCount;
#else
            pipelineBuildInfo.resourceMapping.pUserDataNodes = pResourceMappingNode;
            pipelineBuildInfo.resourceMapping.userDataNodeCount = mappingNodeCount;
#endif

            VK_ASSERT(pColorTarget != 0);
            for (uint32_t i = 0; i < Vkgc::MaxColorTargets; ++i)
            {
                pipelineBuildInfo.cbState.target[i] = pColorTarget[i];
            }
            auto llpcResult = m_pLlpc->BuildGraphicsPipeline(&pipelineBuildInfo, &pipelineOut, nullptr);
            if (llpcResult != Vkgc::Result::Success)
            {
                // There shouldn't be anything to free for the failure case
                VK_ASSERT(pLlpcPipelineBuffer == nullptr);
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
            pInstance->FreeMem(const_cast<void*>(pipelineOut.pipelineBin.pCode));
        }
    }

#if LLPC_CLIENT_INTERFACE_MAJOR_VERSION < 41
    if (pRawNodes != nullptr)
    {
        pInstance->FreeMem(pRawNodes);
    }
#endif

    return result;
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
    VK_IGNORE(pDevice);
    VK_IGNORE(pipelineHash);
    VK_IGNORE(pCacheId);
    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result = VK_SUCCESS;

    // Build the LLPC pipeline
    Llpc::GraphicsPipelineBuildOut  pipelineOut = {};
    void* pLlpcPipelineBuffer = nullptr;

    int64_t startTime = Util::GetPerfCpuTime();
    // Fill pipeline create info for LLPC
    auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
    pPipelineBuildInfo->pInstance      = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
    pPipelineBuildInfo->pUserData      = &pLlpcPipelineBuffer;
    pPipelineBuildInfo->iaState.deviceIndex = deviceIdx;
    if ((pPipelineCache != nullptr) && (settings.shaderCacheMode != ShaderCacheDisable))
    {
        pPipelineBuildInfo->cache = pPipelineCache->GetCacheAdapter();
    }

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
            ppShadersInfo[stage]->options.clientHash.lower = pCreateInfo->pipelineProfileKey.shaders[stage].codeHash.lower;
            ppShadersInfo[stage]->options.clientHash.upper = pCreateInfo->pipelineProfileKey.shaders[stage].codeHash.upper;
        }
    }

    auto llpcResult = m_pLlpc->BuildGraphicsPipeline(pPipelineBuildInfo, &pipelineOut, pPipelineDumpHandle);
    pCreateInfo->pipelineFeedback = {};
    memset(pCreateInfo->stageFeedback, 0, sizeof(pCreateInfo->stageFeedback));
    if (llpcResult != Vkgc::Result::Success)
    {
        // There shouldn't be anything to free for the failure case
        VK_ASSERT(pLlpcPipelineBuffer == nullptr);
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
                                    pPipelineBuildInfo->fs,
                                    pipelineOut.stageCacheAccesses,
                                    ShaderStage::ShaderStageFragment);
    }

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            char extraInfo[256];
            ShaderOptimizerKey* pShaderKey = &pCreateInfo->pipelineProfileKey.shaders[0];
            Util::Snprintf(extraInfo, sizeof(extraInfo), "\n;PipelineOptimizer\n");
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
            for (uint32_t i = 0; i < ShaderStageCount; i++)
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
    VK_IGNORE(pDevice);
    VK_IGNORE(pipelineHash);
    VK_IGNORE(pCacheId);

    const RuntimeSettings& settings = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    AppProfile             appProfile = m_pPhysicalDevice->GetAppProfile();

    Vkgc::ComputePipelineBuildInfo* pPipelineBuildInfo = &pCreateInfo->pipelineInfo;

    VkResult result = VK_SUCCESS;

    int64_t startTime = Util::GetPerfCpuTime();

    // Build the LLPC pipeline
    Llpc::ComputePipelineBuildOut  pipelineOut         = {};
    void*                          pLlpcPipelineBuffer = nullptr;

    // Fill pipeline create info for LLPC
    pPipelineBuildInfo->pInstance      = pInstance;
    pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
    pPipelineBuildInfo->pUserData      = &pLlpcPipelineBuffer;
    if ((pPipelineCache != nullptr) && (settings.shaderCacheMode != ShaderCacheDisable))
    {
        pPipelineBuildInfo->cache = pPipelineCache->GetCacheAdapter();
    }

    // Force enable automatic workgroup reconfigure.
    if (appProfile == AppProfile::DawnOfWarIII)
    {
        pPipelineBuildInfo->options.reconfigWorkgroupLayout = true;
    }

    // By default the client hash provided to PAL is more accurate than the one used by pipeline
    // profiles.
    //
    // Optionally (based on panel setting), these can be set to temporarily match by devs.  This
    // can be useful when other tools (such as PAL's profiling layer) are used to measure shaders
    // while building a pipeline profile which uses the profile hash.
    if (settings.pipelineUseProfileHashAsClientHash)
    {
        pPipelineBuildInfo->cs.options.clientHash.lower =
            pCreateInfo->pipelineProfileKey.shaders[ShaderStage::ShaderStageCompute].codeHash.lower;
        pPipelineBuildInfo->cs.options.clientHash.upper =
            pCreateInfo->pipelineProfileKey.shaders[ShaderStage::ShaderStageCompute].codeHash.upper;
    }

    // Build pipline binary
    auto llpcResult = m_pLlpc->BuildComputePipeline(pPipelineBuildInfo, &pipelineOut, pPipelineDumpHandle);
    pCreateInfo->pipelineFeedback = {};
    pCreateInfo->stageFeedback = {};
    if (llpcResult != Vkgc::Result::Success)
    {
        // There shouldn't be anything to free for the failure case
        VK_ASSERT(pLlpcPipelineBuffer == nullptr);
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
    VK_ASSERT(*ppPipelineBinary == pLlpcPipelineBuffer);

    if (settings.enablePipelineDump && (pPipelineDumpHandle != nullptr))
    {
        if (result == VK_SUCCESS)
        {
            char extraInfo[256];
            ShaderOptimizerKey* pShaderKey = &pCreateInfo->pipelineProfileKey.shaders[ShaderStage::ShaderStageCompute];
            Util::Snprintf(extraInfo, sizeof(extraInfo), "\n\n;PipelineOptimizer\n");
            Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);

            if (pShaderKey->codeHash.upper || pShaderKey->codeHash.lower)
            {
                const char* pName = GetShaderStageName(ShaderStage::ShaderStageCompute);
                Util::Snprintf(
                    extraInfo,
                    sizeof(extraInfo),
                    ";%s Shader Profile Key: 0x%016" PRIX64 "%016" PRIX64 ",\n",
                    pName,
                    pShaderKey->codeHash.upper,
                    pShaderKey->codeHash.lower);
                Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);
            }
        }
    }

    *pCompileTime = Util::GetPerfCpuTime() - startTime;

    return result;
}

// =====================================================================================================================
void CompilerSolutionLlpc::FreeGraphicsPipelineBinary(
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    VK_IGNORE(binarySize);
    m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
}

// =====================================================================================================================
void CompilerSolutionLlpc::FreeComputePipelineBinary(
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    VK_IGNORE(binarySize);
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

    // Enable shadow descriptor table
    Pal::DeviceProperties info;
    m_pPhysicalDevice->PalDevice()->GetProperties(&info);

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

    // Generate ELF binary, not assembly text
    llpcOptions[numOptions++] = "-filetype=obj";

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
    llpcOptions[numOptions++] = "-simplifycfg-sink-common=false";
    llpcOptions[numOptions++] = "-amdgpu-vgpr-index-mode"; // force VGPR indexing on GFX8

    llpcOptions[numOptions++] = "-amdgpu-atomic-optimizations";
    llpcOptions[numOptions++] = "-use-gpu-divergence-analysis";

    llpcOptions[numOptions++] = "-enable-load-scalarizer";
    llpcOptions[numOptions++] = "-scalar-threshold=3";

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

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-executable-name=%s", pExecutablePtr);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-cache-file-dir=%s", m_pPhysicalDevice->PalDevice()->GetCacheFilePath());
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-cache-mode=%d", shaderCacheMode);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-subgroup-size=%d", m_pPhysicalDevice->GetSubgroupSize());
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    if ((m_gfxIp.major == 10) && (m_gfxIp.minor >= 3))
    {
        // Enable flat scratch for gfx10.3+
        llpcOptions[numOptions++] = "-amdgpu-enable-flat-scratch";
    }

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
