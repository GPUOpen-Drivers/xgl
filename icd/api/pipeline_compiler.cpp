/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  pipeline_compiler.cpp
 * @brief Contains implementation of Vulkan pipeline compiler
 ***********************************************************************************************************************
 */

#include "include/pipeline_compiler.h"
#include "include/vk_device.h"
#include "include/vk_shader.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"

namespace vk
{

extern bool IsSrcAlphaUsedInBlend(VkBlendFactor blend);

// =====================================================================================================================
ShaderCache::ShaderCache()
    :
    m_cacheType(),
    m_cache()
{

}

// =====================================================================================================================
// Initializes shader cache object.
void ShaderCache::Init(
    PipelineCacheType  cacheType,  // Shader cache type
    ShaderCachePtr     cachePtr)   // Pointer of the shader cache implementation
{
    m_cacheType = cacheType;
    m_cache = cachePtr;
}

// =====================================================================================================================
// Serializes the shader cache data or queries the size required for serialization.
VkResult ShaderCache::Serialize(
    void*   pBlob,
    size_t* pSize)
{
    VkResult result = VK_SUCCESS;

    {
        Llpc::Result llpcResult = m_cache.pLlpcShaderCache->Serialize(pBlob, pSize);
        result = (llpcResult == Llpc::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Merges the provided source shader caches' content into this shader cache.
VkResult ShaderCache::Merge(
    uint32_t              srcCacheCount,
    const ShaderCachePtr* ppSrcCaches)
{
    VkResult result = VK_SUCCESS;

    {
        Llpc::Result llpcResult = m_cache.pLlpcShaderCache->Merge(srcCacheCount,
            const_cast<const Llpc::IShaderCache **>(&ppSrcCaches->pLlpcShaderCache));
        result = (llpcResult == Llpc::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
// Frees all resources associated with this object.
void ShaderCache::Destroy(
    PipelineCompiler* pCompiler)
{
    VkResult result = VK_SUCCESS;

    {
        if (m_cache.pLlpcShaderCache)
        {
            m_cache.pLlpcShaderCache->Destroy();
        }
    }
}

// =====================================================================================================================
PipelineCompiler::PipelineCompiler(PhysicalDevice* pPhysicalDevice)
    :
    m_pPhysicalDevice(pPhysicalDevice)
    , m_pLlpc(nullptr)
{

}

// =====================================================================================================================
PipelineCompiler::~PipelineCompiler()
{
    VK_ASSERT(m_pLlpc == nullptr);
}

// =====================================================================================================================
// Initializes pipeline compiler.
VkResult PipelineCompiler::Initialize()
{
    Pal::IDevice* pPalDevice = m_pPhysicalDevice->PalDevice();

    // Initialzie GfxIp informations per PAL device properties
    Pal::DeviceProperties info;
    pPalDevice->GetProperties(&info);
    m_gfxIpLevel = info.gfxLevel;

    switch (info.gfxLevel)
    {
    case Pal::GfxIpLevel::GfxIp6:
        m_gfxIp.major = 6;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp7:
        m_gfxIp.major = 7;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp8:
        m_gfxIp.major = 8;
        m_gfxIp.minor = 0;
        break;
    case Pal::GfxIpLevel::GfxIp8_1:
        m_gfxIp.major = 8;
        m_gfxIp.minor = 1;
        break;
    case Pal::GfxIpLevel::GfxIp9:
        m_gfxIp.major = 9;
        m_gfxIp.minor = 0;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    m_gfxIp.stepping = info.gfxStepping;

    // Create compiler objects
    VkResult result = VK_SUCCESS;
    result = CreateLlpcCompiler();

    return result;
}

// =====================================================================================================================
// Destroies all compiler instance.
void PipelineCompiler::Destroy()
{
    if (m_pLlpc)
    {
        m_pLlpc->Destroy();
        m_pLlpc = nullptr;
    }

}

// =====================================================================================================================
// Creates LLPC compiler instance.
VkResult PipelineCompiler::CreateLlpcCompiler()
{
    const uint32_t         OptionBufferSize = 4096;
    const uint32_t         MaxLlpcOptions   = 32;
    Llpc::ICompiler*       pCompiler        = nullptr;
    const RuntimeSettings& settings         = m_pPhysicalDevice->GetRuntimeSettings();
#ifdef ICD_BUILD_APPPROFILE
    AppProfile             appProfile       = m_pPhysicalDevice->GetAppProfile();
#endif
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

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-log-file-outs=%s", settings.logFileName);
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

    if (settings.llpcOptions[0] != '\0')
    {
        const char* pOptions = &settings.llpcOptions[0];
        VK_ASSERT(pOptions[0] == '-');

        // Split options
        while (pOptions)
        {
            const char* pNext = strchr(pOptions, ' ');
            if (pNext)
            {
                // Copy options to option buffer
                optionLength = static_cast<int32_t>(pNext - pOptions);
                memcpy(pOptionBuffer, pOptions, optionLength);
                pOptionBuffer[optionLength] = 0;

                llpcOptions[numOptions++] = pOptionBuffer;
                pOptionBuffer += (optionLength + 1);

                bufSize -= (optionLength + 1);
                pOptions = strchr(pOptions + optionLength, '-');
            }
            else
            {
                // Use pOptions directly for last option
                llpcOptions[numOptions++] = pOptions;
                pOptions = nullptr;
            }
        }
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

    if (settings.enableLlpc == LlpcModeAutoFallback)
    {
        llpcOptions[numOptions++] = "-disable-WIP-features=1";
    }

    // NOTE: For testing consistency, these options should be kept the same as those of
    // "amdllpc" (Init()).
    llpcOptions[numOptions++] = "-pragma-unroll-threshold=4096";
    llpcOptions[numOptions++] = "-unroll-allow-partial";
    llpcOptions[numOptions++] = "-lower-dyn-index";
    llpcOptions[numOptions++] = "-simplifycfg-sink-common=false";
    llpcOptions[numOptions++] = "-amdgpu-vgpr-index-mode"; // force VGPR indexing on GFX8

    ShaderCacheMode shaderCacheMode = settings.shaderCacheMode;
#ifdef ICD_BUILD_APPPROFILE
    if ((appProfile == AppProfile::Talos) ||
        (appProfile == AppProfile::MadMax) ||
        (appProfile == AppProfile::SeriousSamFusion))
    {
        llpcOptions[numOptions++] = "-enable-si-scheduler";
    }

    // Force enable cache to disk to improve user experience
    if ((shaderCacheMode == ShaderCacheEnableRuntimeOnly) &&
         ((appProfile == AppProfile::MadMax) ||
          (appProfile == AppProfile::SeriousSamFusion) ||
          (appProfile == AppProfile::F1_2017)))
    {
        // Force to use internal disk cache.
        shaderCacheMode = ShaderCacheForceInternalCacheOnDisk;
    }
#endif

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-executable-name=%s", pExecutablePtr);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-cache-mode=%d", shaderCacheMode);
    ++optionLength;
    llpcOptions[numOptions++] = pOptionBuffer;
    pOptionBuffer += optionLength;
    bufSize -= optionLength;

    if (settings.shaderReplaceMode != 0)
    {
        optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-replace-mode=%d", settings.shaderReplaceMode);
        ++optionLength;
        llpcOptions[numOptions++] = pOptionBuffer;
        pOptionBuffer += optionLength;
        bufSize -= optionLength;

        optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-replace-dir=%s", settings.shaderReplaceDir);
        ++optionLength;
        llpcOptions[numOptions++] = pOptionBuffer;
        pOptionBuffer += optionLength;
        bufSize -= optionLength;

        optionLength = Util::Snprintf(pOptionBuffer, bufSize, "-shader-replace-pipeline-hashes=%s", settings.shaderReplacePipelineHashes);
        ++optionLength;
        llpcOptions[numOptions++] = pOptionBuffer;
        pOptionBuffer += optionLength;
        bufSize -= optionLength;
    }

    VK_ASSERT(numOptions <= MaxLlpcOptions);

    // Create LLPC compiler
    Llpc::Result llpcResult = Llpc::ICompiler::Create(m_gfxIp, numOptions, llpcOptions, &pCompiler);
    VK_ASSERT(llpcResult == Llpc::Result::Success);

    m_pLlpc = pCompiler;

    return (llpcResult == Llpc::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Creates shader cache object.
VkResult PipelineCompiler::CreateShaderCache(
    const void*   pInitialData,
    size_t        initialDataSize,
    void*         pShaderCacheMem,
    bool          isScpcInternalCache,
    ShaderCache*  pShaderCache)
{
    VkResult                     result         = VK_SUCCESS;
    const RuntimeSettings        settings       = m_pPhysicalDevice->GetRuntimeSettings();
    PipelineCacheType            cacheType      = GetShaderCacheType();
    ShaderCache::ShaderCachePtr  shaderCachePtr = {};

    {
        // Create shader cache for LLPC
        Llpc::ShaderCacheCreateInfo llpcCacheCreateInfo = {};

        llpcCacheCreateInfo.pInitialData    = pInitialData;
        llpcCacheCreateInfo.initialDataSize = initialDataSize;

        Llpc::Result llpcResult = m_pLlpc->CreateShaderCache(
            &llpcCacheCreateInfo,
            &shaderCachePtr.pLlpcShaderCache);

        if (llpcResult == Llpc::Result::Success)
        {
            pShaderCache->Init(PipelineCacheTypeLlpc, shaderCachePtr);
        }
        else
        {
            result = VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return result;
}

// =====================================================================================================================
// Gets the size of shader cache object.
size_t PipelineCompiler::GetShaderCacheSize(PipelineCacheType cacheType)
{
    size_t shaderCacheSize = 0;
    return shaderCacheSize;
}

// =====================================================================================================================
// Gets shader cache type.
PipelineCacheType PipelineCompiler::GetShaderCacheType()
{
    PipelineCacheType cacheType;
    cacheType = PipelineCacheTypeLlpc;
    return cacheType;
}

// =====================================================================================================================
// Builds shader module from SPIR-V binary code.
VkResult PipelineCompiler::BuildShaderModule(
    size_t          codeSize,            ///< Size of shader binary data
    const void*     pCode                ///< Shader binary data
    , void**          ppLlpcShaderModule ///< [out] LLPC shader module
    )
{
    const RuntimeSettings* pSettings = &m_pPhysicalDevice->GetRuntimeSettings();
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    VkResult result = VK_SUCCESS;
    {
        if (ppLlpcShaderModule != nullptr)
        {
            // Build LLPC shader module
            Llpc::ShaderModuleBuildInfo  moduleInfo = {};
            Llpc::ShaderModuleBuildOut   buildOut = {};
            void* pShaderMemory = nullptr;

            moduleInfo.pInstance      = pInstance;
            moduleInfo.pfnOutputAlloc = AllocateShaderOutput;
            moduleInfo.pUserData      = &pShaderMemory;
            moduleInfo.shaderBin.pCode    = pCode;
            moduleInfo.shaderBin.codeSize = codeSize;

            Llpc::Result llpcResult = m_pLlpc->BuildShaderModule(&moduleInfo, &buildOut);

            if ((llpcResult == Llpc::Result::Success) || (llpcResult == Llpc::Result::Delayed))
            {
                *ppLlpcShaderModule = buildOut.pModuleData;
                VK_ASSERT(pShaderMemory == *ppLlpcShaderModule);
            }
            else
            {
                // Clean up if fail
                pInstance->FreeMem(pShaderMemory);
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Frees shader module memory
void PipelineCompiler::FreeShaderModule(
    void* pShaderModule)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    pInstance->FreeMem(pShaderModule);
}

// =====================================================================================================================
// Creates graphics pipeline binary.
VkResult PipelineCompiler::CreateGraphicsPipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    GraphicsPipelineCreateInfo*         pCreateInfo,
    size_t*                             pPipelineBinarySize,
    const void**                        ppPipelineBinary)
{
    VkResult               result    = VK_SUCCESS;
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    // Build the LLPC pipeline
    Llpc::GraphicsPipelineBuildOut  pipelineOut         = {};
    void*                           pLlpcPipelineBuffer = nullptr;

    {
        // Fill pipeline create info for LLPC
        auto pPipelineBuildInfo = &pCreateInfo->pipelineInfo;
        pPipelineBuildInfo->pInstance      = pInstance;
        pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
        pPipelineBuildInfo->pUserData      = &pLlpcPipelineBuffer;
        pPipelineBuildInfo->iaState.deviceIndex = deviceIdx;

        if (pPipelineCache != nullptr)
        {
            if (pPipelineCache->GetShaderCache(deviceIdx).GetCacheType() == PipelineCacheTypeLlpc)
            {
                pPipelineBuildInfo->pShaderCache =
                    pPipelineCache->GetShaderCache(deviceIdx).GetCachePtr().pLlpcShaderCache;
            }
        }

        auto llpcResult = m_pLlpc->BuildGraphicsPipeline(pPipelineBuildInfo, &pipelineOut);
        if (llpcResult != Llpc::Result::Success)
        {
            // There shouldn't be anything to free for the failure case
            VK_ASSERT(pLlpcPipelineBuffer == nullptr);

            {
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
        }
        else
        {
            *ppPipelineBinary   = pipelineOut.pipelineBin.pCode;
            *pPipelineBinarySize = pipelineOut.pipelineBin.codeSize;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates compute pipeline binary.
VkResult PipelineCompiler::CreateComputePipelineBinary(
    Device*                             pDevice,
    uint32_t                            deviceIdx,
    PipelineCache*                      pPipelineCache,
    ComputePipelineCreateInfo*          pCreateInfo,
    size_t*                             pPipelineBinarySize,
    const void**                        ppPipelineBinary)
{
    VkResult               result    = VK_SUCCESS;
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    // Build the LLPC pipeline
    Llpc::ComputePipelineBuildOut  pipelineOut         = {};
    void*                          pLlpcPipelineBuffer = nullptr;

    {
        // Fill pipeline create info for LLPC
        Llpc::ComputePipelineBuildInfo* pPipelineBuildInfo = &pCreateInfo->pipelineInfo;

        pPipelineBuildInfo->pInstance      = pInstance;
        pPipelineBuildInfo->pfnOutputAlloc = AllocateShaderOutput;
        pPipelineBuildInfo->pUserData      = &pLlpcPipelineBuffer;
        pPipelineBuildInfo->deviceIndex    = deviceIdx;

        if (pPipelineCache != nullptr)
        {
            if (pPipelineCache->GetShaderCache(deviceIdx).GetCacheType() == PipelineCacheTypeLlpc)
            {
                pPipelineBuildInfo->pShaderCache =
                    pPipelineCache->GetShaderCache(deviceIdx).GetCachePtr().pLlpcShaderCache;
            }
        }

        // Build pipline binary
        auto llpcResult = m_pLlpc->BuildComputePipeline(pPipelineBuildInfo, &pipelineOut);
        if (llpcResult != Llpc::Result::Success)
        {
            // There shouldn't be anything to free for the failure case
            VK_ASSERT(pLlpcPipelineBuffer == nullptr);

            {
                result = VK_ERROR_INITIALIZATION_FAILED;
            }
        }
        else
        {
            *ppPipelineBinary   = pipelineOut.pipelineBin.pCode;
            *pPipelineBinarySize = pipelineOut.pipelineBin.codeSize;
        }
        VK_ASSERT(*ppPipelineBinary == pLlpcPipelineBuffer);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertGraphicsPipelineInfo(
    Device*                             pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    GraphicsPipelineCreateInfo*         pCreateInfo,
    VbBindingInfo*                      pVbInfo)
{
    VkResult               result    = VK_SUCCESS;
    const RuntimeSettings& settings  = m_pPhysicalDevice->GetRuntimeSettings();
    auto                   pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    EXTRACT_VK_STRUCTURES_0(
        gfxPipeline,
        GraphicsPipelineCreateInfo,
        pIn,
        GRAPHICS_PIPELINE_CREATE_INFO)

    // Fill in necessary non-zero defaults in case some information is missing
    const RenderPass* pRenderPass = nullptr;
    const PipelineLayout* pLayout = nullptr;
    const VkPipelineShaderStageCreateInfo* pStageInfos[ShaderGfxStageCount] = {};

    if (pGraphicsPipelineCreateInfo != nullptr)
    {
        for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
        {
            ShaderStage stage = ShaderFlagBitToStage(pGraphicsPipelineCreateInfo->pStages[i].stage);
            VK_ASSERT(stage < ShaderGfxStageCount);
            pStageInfos[stage] = &pGraphicsPipelineCreateInfo->pStages[i];
        }

        VK_IGNORE(pGraphicsPipelineCreateInfo->flags & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT);

        pRenderPass = RenderPass::ObjectFromHandle(pGraphicsPipelineCreateInfo->renderPass);

        if (pGraphicsPipelineCreateInfo->layout != VK_NULL_HANDLE)
        {
            pLayout = PipelineLayout::ObjectFromHandle(pGraphicsPipelineCreateInfo->layout);
        }

        pCreateInfo->pipelineInfo.pVertexInput = pGraphicsPipelineCreateInfo->pVertexInputState;

        const VkPipelineInputAssemblyStateCreateInfo* pIa = pGraphicsPipelineCreateInfo->pInputAssemblyState;
        // According to the spec this should never be null
        VK_ASSERT(pIa != nullptr);

        pCreateInfo->pipelineInfo.iaState.enableMultiView = pRenderPass->IsMultiviewEnabled();
        pCreateInfo->pipelineInfo.iaState.topology           = pIa->topology;
        pCreateInfo->pipelineInfo.iaState.disableVertexReuse = false;

        EXTRACT_VK_STRUCTURES_1(
            Tess,
            PipelineTessellationStateCreateInfo,
            PipelineTessellationDomainOriginStateCreateInfoKHR,
            pGraphicsPipelineCreateInfo->pTessellationState,
            PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO_KHR)

        if (pPipelineTessellationStateCreateInfo != nullptr)
        {
            pCreateInfo->pipelineInfo.iaState.patchControlPoints = pPipelineTessellationStateCreateInfo->patchControlPoints;
        }

        if (pPipelineTessellationDomainOriginStateCreateInfoKHR)
        {
            // Vulkan 1.0 incorrectly specified the tessellation u,v coordinate origin as lower left even though
            // framebuffer and image coordinate origins are in the upper left.  This has since been fixed, but
            // an extension exists to use the previous behavior.  Doing so with flat shading would likely appear
            // incorrect, but Vulkan specifies that the provoking vertex is undefined when tessellation is active.
            if (pPipelineTessellationDomainOriginStateCreateInfoKHR->domainOrigin == VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT_KHR)
            {
                pCreateInfo->pipelineInfo.iaState.switchWinding = true;
            }
        }

        const VkPipelineRasterizationStateCreateInfo* pRs = pGraphicsPipelineCreateInfo->pRasterizationState;
        // By default rasterization is disabled, unless rasterization creation info is present
        pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = true;
        if (pRs != nullptr)
        {
            pCreateInfo->pipelineInfo.vpState.depthClipEnable         = (pRs->depthClampEnable == VK_FALSE);
            pCreateInfo->pipelineInfo.rsState.rasterizerDiscardEnable = (pRs->rasterizerDiscardEnable != VK_FALSE);
        }

        bool multisampleEnable = false;
        uint32_t rasterizationSampleCount = 0;
        const VkPipelineMultisampleStateCreateInfo* pMs = pGraphicsPipelineCreateInfo->pMultisampleState;

        pCreateInfo->pipelineInfo.rsState.numSamples = 1;
        if (pMs != nullptr)
        {
            multisampleEnable = (pMs->rasterizationSamples != 1);

            if (multisampleEnable)
            {
                VK_ASSERT(pRenderPass != nullptr);

                rasterizationSampleCount            = pMs->rasterizationSamples;
                uint32_t subpassCoverageSampleCount = pRenderPass->GetSubpassMaxSampleCount(pGraphicsPipelineCreateInfo->subpass);
                uint32_t subpassColorSampleCount    = pRenderPass->GetSubpassColorSampleCount(pGraphicsPipelineCreateInfo->subpass);

                // subpassCoverageSampleCount would be equal to zero if there are zero attachments.
                subpassCoverageSampleCount = subpassCoverageSampleCount == 0 ? rasterizationSampleCount : subpassCoverageSampleCount;

                subpassColorSampleCount = subpassColorSampleCount == 0 ? subpassCoverageSampleCount : subpassColorSampleCount;

                if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
                {
                    pCreateInfo->pipelineInfo.rsState.perSampleShading =((subpassColorSampleCount * pMs->minSampleShading) > 1.0f);
                }
                else
                {
                    pCreateInfo->pipelineInfo.rsState.perSampleShading = false;
                }

                pCreateInfo->pipelineInfo.rsState.numSamples = rasterizationSampleCount;

                // NOTE: The sample pattern index here is actually the offset of sample position pair. This is
                // different from the field of creation info of image view. For image view, the sample pattern
                // index is really table index of the sample pattern.
                pCreateInfo->pipelineInfo.rsState.samplePatternIdx =
                    Device::GetDefaultSamplePatternIndex(subpassCoverageSampleCount) * Pal::MaxMsaaRasterizerSamples;
            }
            pCreateInfo->pipelineInfo.cbState.alphaToCoverageEnable = (pMs->alphaToCoverageEnable == VK_TRUE);
        }

        const VkPipelineColorBlendStateCreateInfo* pCb = pGraphicsPipelineCreateInfo->pColorBlendState;
        bool dualSourceBlend = false;

        if (pCb != nullptr)
        {
            const uint32_t numColorTargets = Util::Min(pCb->attachmentCount, Pal::MaxColorTargets);

            for (uint32_t i = 0; i < numColorTargets; ++i)
            {
                const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];
                auto pLlpcCbDst = &pCreateInfo->pipelineInfo.cbState.target[i];
                if (pRenderPass)
                {
                    auto cbFormat = pRenderPass->GetColorAttachmentFormat(pGraphicsPipelineCreateInfo->subpass, i);
                    // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
                    // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
                    // disable shader writes through that target.
                    if (cbFormat != VK_FORMAT_UNDEFINED)
                    {
                        pLlpcCbDst->format               = cbFormat;
                        pLlpcCbDst->blendEnable          = (src.blendEnable == VK_TRUE);
                        pLlpcCbDst->blendSrcAlphaToColor = IsSrcAlphaUsedInBlend(src.srcAlphaBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.dstAlphaBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.srcColorBlendFactor) ||
                                                           IsSrcAlphaUsedInBlend(src.dstColorBlendFactor);
                        pLlpcCbDst->channelWriteMask     = src.colorWriteMask;
                    }
                }

                dualSourceBlend |= IsDualSourceBlend(src.srcAlphaBlendFactor);
                dualSourceBlend |= IsDualSourceBlend(src.dstAlphaBlendFactor);
                dualSourceBlend |= IsDualSourceBlend(src.srcColorBlendFactor);
                dualSourceBlend |= IsDualSourceBlend(src.dstColorBlendFactor);
            }
        }
        pCreateInfo->pipelineInfo.cbState.dualSourceBlendEnable = dualSourceBlend;

        VkFormat dbFormat = { };
        if (pRenderPass != nullptr)
        {
            dbFormat = pRenderPass->GetDepthStencilAttachmentFormat(pGraphicsPipelineCreateInfo->subpass);
            pCreateInfo->dbFormat = dbFormat;
        }
    }

    // Allocate space to create the LLPC/SCPC pipeline resource mappings
    if (pLayout != nullptr)
    {
        pCreateInfo->tempBufferStageSize = pLayout->GetPipelineInfo()->tempStageSize;
        size_t tempBufferSize = pLayout->GetPipelineInfo()->tempBufferSize;

        // Allocate the temp buffer
        if (tempBufferSize > 0)
        {
            pCreateInfo->pMappingBuffer = pInstance->AllocMem(
                tempBufferSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pCreateInfo->pMappingBuffer == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
                // fields of resource mapping nodes are unused for certain node types. We must initialize
                // them to zeroes.
                memset(pCreateInfo->pMappingBuffer, 0, tempBufferSize);
            }
        }
    }

    // Build the LLPC pipeline
    Llpc::PipelineShaderInfo* shaderInfos[] =
    {
        &pCreateInfo->pipelineInfo.vs,
        &pCreateInfo->pipelineInfo.tcs,
        &pCreateInfo->pipelineInfo.tes,
        &pCreateInfo->pipelineInfo.gs,
        &pCreateInfo->pipelineInfo.fs
    };

    // Apply patches
    pCreateInfo->pipelineInfo.pInstance      = pInstance;
    pCreateInfo->pipelineInfo.pfnOutputAlloc = AllocateShaderOutput;

    for (uint32_t stage = 0; stage < ShaderGfxStageCount; ++stage)
    {
        auto pStage = pStageInfos[stage];
        auto pShaderInfo = shaderInfos[stage];

        if (pStage == nullptr)
            continue;

        auto pShaderModule = ShaderModule::ObjectFromHandle(pStage->module);
        pCreateInfo->pShaderModules[stage] = pShaderModule;
        pShaderInfo->pModuleData           = pShaderModule->GetShaderData(true);
        pShaderInfo->pSpecializationInfo   = pStage->pSpecializationInfo;
        pShaderInfo->pEntryTarget          = pStage->pName;

        // Build the resource mapping description for LLPC.  This data contains things about how shader
        // inputs like descriptor set bindings are communicated to this pipeline in a form that LLPC can
        // understand.
        if (pLayout != nullptr)
        {
            const bool vertexShader = (stage == ShaderStageVertex);
            result = pLayout->BuildLlpcPipelineMapping(
                static_cast<ShaderStage>(stage),
                pCreateInfo->pMappingBuffer,
                vertexShader ? pCreateInfo->pipelineInfo.pVertexInput : nullptr,
                pShaderInfo,
                vertexShader ? pVbInfo : nullptr);
        }
    }

    return result;
}

// =====================================================================================================================
// Checks whether dual source blend is needed.
bool PipelineCompiler::IsDualSourceBlend(
    VkBlendFactor blend)
{
    bool result = false;
    switch(blend)
    {
    case VK_BLEND_FACTOR_SRC1_COLOR:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
    case VK_BLEND_FACTOR_SRC1_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        result = true;
        break;
    default:
        result = false;
        break;
    }
    return result;
}

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
VkResult PipelineCompiler::ConvertComputePipelineInfo(
    const VkComputePipelineCreateInfo*  pIn,
    ComputePipelineCreateInfo*          pCreateInfo)
{
    VkResult result    = VK_SUCCESS;
    auto     pInstance = m_pPhysicalDevice->Manager()->VkInstance();
    PipelineLayout* pLayout = nullptr;
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }
    pCreateInfo->flags  = pIn->flags;

    // Allocate space to create the LLPC/SCPC pipeline resource mappings
    if (pLayout != nullptr)
    {
        pCreateInfo->tempBufferStageSize = pLayout->GetPipelineInfo()->tempStageSize;
        size_t tempBufferSize = pLayout->GetPipelineInfo()->tempBufferSize;

        // Allocate the temp buffer
        if (tempBufferSize > 0)
        {
            pCreateInfo->pMappingBuffer = pInstance->AllocMem(
                tempBufferSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pCreateInfo->pMappingBuffer == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            else
            {
                // NOTE: Zero the allocated space that is used to create pipeline resource mappings. Some
                // fields of resource mapping nodes are unused for certain node types. We must initialize
                // them to zeroes.
                memset(pCreateInfo->pMappingBuffer, 0, tempBufferSize);
            }
        }
    }

     ShaderModule* pShaderModule = ShaderModule::ObjectFromHandle(pIn->stage.module);
     pCreateInfo->pShaderModule = pShaderModule;
     pCreateInfo->pipelineInfo.cs.pModuleData         = pShaderModule->GetShaderData(true);
     pCreateInfo->pipelineInfo.cs.pSpecializationInfo = pIn->stage.pSpecializationInfo;
     pCreateInfo->pipelineInfo.cs.pEntryTarget        = pIn->stage.pName;

    // Build the resource mapping description for LLPC.  This data contains things about how shader
    // inputs like descriptor set bindings interact with this pipeline in a form that LLPC can
    // understand.
    if (pLayout != nullptr)
    {
        result = pLayout->BuildLlpcPipelineMapping(
            ShaderStageCompute,
            pCreateInfo->pMappingBuffer,
            nullptr,
            &pCreateInfo->pipelineInfo.cs,
            nullptr);
    }

    return result;
}

// =====================================================================================================================
// Free compute pipeline binary
void PipelineCompiler::FreeComputePipelineBinary(
    ComputePipelineCreateInfo* pCreateInfo,
    const void*                pPipelineBinary,
    size_t                     binarySize)
{
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
}

// =====================================================================================================================
// Free graphics pipeline binary
void PipelineCompiler::FreeGraphicsPipelineBinary(
    GraphicsPipelineCreateInfo* pCreateInfo,
    const void*                 pPipelineBinary,
    size_t                      binarySize)
{
    {
        m_pPhysicalDevice->Manager()->VkInstance()->FreeMem(const_cast<void*>(pPipelineBinary));
    }
}

// =====================================================================================================================
// Free the temp memories in compute pipeline create info
void PipelineCompiler::FreeComputePipelineCreateInfo(
    ComputePipelineCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pMappingBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pMappingBuffer);
        pCreateInfo->pMappingBuffer = nullptr;
    }
}

// =====================================================================================================================
// Free the temp memories in graphics pipeline create info
void PipelineCompiler::FreeGraphicsPipelineCreateInfo(
    GraphicsPipelineCreateInfo* pCreateInfo)
{
    auto pInstance = m_pPhysicalDevice->Manager()->VkInstance();

    if (pCreateInfo->pMappingBuffer != nullptr)
    {
        pInstance->FreeMem(pCreateInfo->pMappingBuffer);
        pCreateInfo->pMappingBuffer = nullptr;
    }
}

}

