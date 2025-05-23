/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/log.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_graphics_pipeline_library.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_pipeline_binary.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_shader.h"
#include "include/vk_cmdbuffer.h"
#if VKI_RAY_TRACING
#include "raytrace/ray_tracing_device.h"
#endif

#include "palAutoBuffer.h"
#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palPipeline.h"
#include "palInlineFuncs.h"
#include "palMetroHash.h"
#include "palVectorImpl.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>

using namespace Util;
using namespace std::chrono_literals;

namespace vk
{

// =====================================================================================================================
// Create graphics pipeline binaries
VkResult GraphicsPipeline::CreatePipelineBinaries(
    Device*                                        pDevice,
    const VkGraphicsPipelineCreateInfo*            pCreateInfo,
    const GraphicsPipelineExtStructs&              extStructs,
    const GraphicsPipelineLibraryInfo&             libInfo,
    VkPipelineCreateFlags2KHR                      flags,
    const GraphicsPipelineShaderStageInfo*         pShaderInfo,
    const PipelineResourceLayout*                  pResourceLayout,
    const PipelineOptimizerKey*                    pPipelineOptimizerKey,
    GraphicsPipelineBinaryCreateInfo*              pBinaryCreateInfo,
    PipelineCache*                                 pPipelineCache,
    const VkPipelineCreationFeedbackCreateInfoEXT* pCreationFeedbackInfo,
    Util::MetroHash::Hash*                         pCacheIds,
    Vkgc::BinaryData*                              pPipelineBinaries,
    PipelineMetadata*                              pBinaryMetadata)
{
    VkResult          result             = VK_SUCCESS;
    const uint32_t    numPalDevices      = pDevice->NumPalDevices();
    PipelineCompiler* pDefaultCompiler   = pDevice->GetCompiler(DefaultDeviceIndex);
    bool              storeBinaryToCache = true;

    storeBinaryToCache = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) == 0;

    // Load or create the pipeline binary
    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

    for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < numPalDevices); ++deviceIdx)
    {
        bool isUserCacheHit     = false;
        bool isInternalCacheHit = false;
        bool shouldCompile      = true;

        if (shouldCompile)
        {
            bool skipCacheQuery = false;
            if (skipCacheQuery == false)
            {
                // Search the pipeline binary cache
                Util::Result cacheResult = pDevice->GetCompiler(deviceIdx)->GetCachedPipelineBinary(
                    &pCacheIds[deviceIdx],
                    pPipelineBinaryCache,
                    &pPipelineBinaries[deviceIdx],
                    &isUserCacheHit,
                    &isInternalCacheHit,
                    &pBinaryCreateInfo->freeCompilerBinary,
                    &pBinaryCreateInfo->pipelineFeedback);

                // Compile if not found in cache
                shouldCompile = (cacheResult != Util::Result::Success);
            }
        }

        if (shouldCompile)
        {
            if ((pDevice->GetRuntimeSettings().ignoreFlagFailOnPipelineCompileRequired == false) &&
                (flags & VK_PIPELINE_CREATE_2_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT_KHR))
            {
                result = VK_PIPELINE_COMPILE_REQUIRED_EXT;
            }
        }

        bool shouldConvert = (pCreateInfo != nullptr) &&
            (pDevice->GetRuntimeSettings().enablePipelineDump ||
             (shouldCompile && (deviceIdx == DefaultDeviceIndex)));

        VkResult convertResult = VK_ERROR_UNKNOWN;
        if (shouldConvert)
        {
            convertResult = pDefaultCompiler->ConvertGraphicsPipelineInfo(
                pDevice,
                pCreateInfo,
                extStructs,
                libInfo,
                flags,
                pShaderInfo,
                pResourceLayout,
                pPipelineOptimizerKey,
                pBinaryMetadata,
                pBinaryCreateInfo);

            if (convertResult == VK_SUCCESS)
            {
                PipelineCompiler::ConvertGraphicsPipelineExecutableState(
                    pDevice,
                    pCreateInfo,
                    libInfo,
                    flags,
                    pShaderInfo,
                    &pResourceLayout->userDataLayout,
                    pBinaryCreateInfo);
            }

            result = (result == VK_SUCCESS) ? convertResult : result;
        }

        if ((result == VK_SUCCESS) && (convertResult == VK_SUCCESS) && shouldCompile)
        {
            if (IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.vs)   ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.gs)   ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.tcs)  ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.tes)  ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.fs)   ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.task) ||
                IsShaderModuleIdentifier(pBinaryCreateInfo->pipelineInfo.mesh))
            {
                result = VK_ERROR_UNKNOWN;
            }
        }

        if (pDevice->GetRuntimeSettings().enablePipelineDump && (convertResult == VK_SUCCESS))
        {
            if ((shouldCompile == false) || (result != VK_SUCCESS))
            {
                Vkgc::PipelineBuildInfo pipelineInfo = {};
                pipelineInfo.pGraphicsInfo = &pBinaryCreateInfo->pipelineInfo;
                pDefaultCompiler->DumpPipeline(
                    pDevice->GetRuntimeSettings(),
                    pipelineInfo,
                    pBinaryCreateInfo->apiPsoHash,
                    1,
                    &pPipelineBinaries[deviceIdx],
                    result);
            }
        }

        // Compile if unable to retrieve from cache
        if (shouldCompile)
        {
            if (result == VK_SUCCESS)
            {
                if ((deviceIdx == DefaultDeviceIndex) || (pCreateInfo == nullptr))
                {
                    result = pDefaultCompiler->CreateGraphicsPipelineBinary(
                        pDevice,
                        deviceIdx,
                        pPipelineCache,
                        pBinaryCreateInfo,
                        flags,
                        &pPipelineBinaries[deviceIdx],
                        &pCacheIds[deviceIdx]);

                    if (result == VK_SUCCESS && (pPipelineBinaries[deviceIdx].codeSize > 0))
                    {
                        result = pDefaultCompiler->WriteBinaryMetadata(
                            pDevice,
                            pBinaryCreateInfo->compilerType,
                            &pBinaryCreateInfo->freeCompilerBinary,
                            &pPipelineBinaries[deviceIdx],
                            pBinaryCreateInfo->pBinaryMetadata);
                    }
                }
                else
                {
                    GraphicsPipelineBinaryCreateInfo binaryCreateInfoMGPU = {};
                    PipelineMetadata                 binaryMetadataMGPU = {};
                    result = pDefaultCompiler->ConvertGraphicsPipelineInfo(
                        pDevice,
                        pCreateInfo,
                        extStructs,
                        libInfo,
                        flags,
                        pShaderInfo,
                        pResourceLayout,
                        pPipelineOptimizerKey,
                        &binaryMetadataMGPU,
                        &binaryCreateInfoMGPU);

                    if (result == VK_SUCCESS)
                    {
                        PipelineCompiler::ConvertGraphicsPipelineExecutableState(
                            pDevice,
                            pCreateInfo,
                            libInfo,
                            flags,
                            pShaderInfo,
                            &pResourceLayout->userDataLayout,
                            &binaryCreateInfoMGPU);

                        result = pDevice->GetCompiler(deviceIdx)->CreateGraphicsPipelineBinary(
                            pDevice,
                            deviceIdx,
                            pPipelineCache,
                            &binaryCreateInfoMGPU,
                            flags,
                            &pPipelineBinaries[deviceIdx],
                            &pCacheIds[deviceIdx]);
                    }

                    if (result == VK_SUCCESS)
                    {
                        result = PipelineCompiler::SetPipelineCreationFeedbackInfo(
                            pCreationFeedbackInfo,
                            pCreateInfo->stageCount,
                            pCreateInfo->pStages,
                            &binaryCreateInfoMGPU.pipelineFeedback,
                            binaryCreateInfoMGPU.stageFeedback);
                    }

                    pDefaultCompiler->FreeGraphicsPipelineCreateInfo(pDevice, &binaryCreateInfoMGPU, false);
                }
            }
        }
        else if (deviceIdx == DefaultDeviceIndex)
        {
            pDefaultCompiler->ReadBinaryMetadata(
                pDevice,
                pPipelineBinaries[DefaultDeviceIndex],
                pBinaryMetadata);
            pBinaryCreateInfo->pBinaryMetadata = pBinaryMetadata;
            PipelineCompiler::UploadInternalBufferData(pDevice, pBinaryCreateInfo);
        }

        // Add to any cache layer where missing
        if ((result == VK_SUCCESS) && storeBinaryToCache)
        {
            // Only store the optimized variant of the pipeline if deferCompileOptimizedPipeline is enabled
            if ((pPipelineBinaries[deviceIdx].codeSize != 0) && (pBinaryMetadata->enableUberFetchShader == false))
            {
                pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                    &pCacheIds[deviceIdx],
                    pPipelineBinaryCache,
                    &pPipelineBinaries[deviceIdx],
                    isUserCacheHit,
                    isInternalCacheHit);
            }
        }

        // insert spirvs into the pipeline binary
        if ((result == VK_SUCCESS) && (pDefaultCompiler->IsEmbeddingSpirvRequired()))
        {
            Vkgc::BinaryData oldElfBinary = pPipelineBinaries[deviceIdx];

            result = PipelineCompiler::InsertSpirvChunkToElf(
                pDevice,
                pCreateInfo->pStages,
                pCreateInfo->stageCount,
                &pPipelineBinaries[deviceIdx]);

            if ((result == VK_SUCCESS) &&
                (oldElfBinary.pCode != nullptr))
            {
                // free old binary and
                // change free-type since the new binary is created with instance allocator
                pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetCompiler()->FreeGraphicsPipelineBinary(
                    pBinaryCreateInfo->compilerType,
                    pBinaryCreateInfo->freeCompilerBinary,
                    oldElfBinary);

                oldElfBinary.pCode = nullptr;
                pBinaryCreateInfo->freeCompilerBinary = FreeWithInstanceAllocator;
            }
            else
            {
                VK_ALERT_ALWAYS_MSG("Failed to insert spirv into the graphics pipeline");
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Creates graphics PAL pipeline objects
VkResult GraphicsPipeline::CreatePalPipelineObjects(
    Device*                           pDevice,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
    const Vkgc::BinaryData*           pPipelineBinaries,
    const Util::MetroHash::Hash*      pCacheIds,
    void*                             pSystemMem,
    Pal::IPipeline**                  pPalPipeline)
{
    size_t palSize = 0;

    Pal::Result palResult = Pal::Result::Success;
    palSize =
        pDevice->PalDevice(DefaultDeviceIndex)->GetGraphicsPipelineSize(pObjectCreateInfo->pipeline, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    RenderStateCache* pRSCache      = pDevice->GetRenderStateCache();
    const uint32_t    numPalDevices = pDevice->NumPalDevices();
    size_t            palOffset     = 0;

    for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
    {
        Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

        if (palResult == Pal::Result::Success)
        {
            // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
            // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
            if (pPipelineBinaries[deviceIdx].pCode != nullptr)
            {
                pObjectCreateInfo->pipeline.pipelineBinarySize = pPipelineBinaries[deviceIdx].codeSize;
                pObjectCreateInfo->pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx].pCode;
            }

            palResult = pPalDevice->CreateGraphicsPipeline(
                pObjectCreateInfo->pipeline,
                Util::VoidPtrInc(pSystemMem, palOffset),
                &pPalPipeline[deviceIdx]);

            // Temporarily reinject post Pal pipeline creation (when the internal pipeline hash is available).
            // The reinjection cache layer can be linked back into the pipeline cache chain once the
            // Vulkan pipeline cache key can be stored (and read back) inside the ELF as metadata.
            if ((pDevice->VkInstance()->GetDevModeMgr() != nullptr) &&
                (palResult == Util::Result::Success))
            {
                const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                palResult = pDevice->GetCompiler(deviceIdx)->RegisterAndLoadReinjectionBinary(
                    &info.internalPipelineHash,
                    &pCacheIds[deviceIdx],
                    &pObjectCreateInfo->pipeline.pipelineBinarySize,
                    &pObjectCreateInfo->pipeline.pPipelineBinary,
                    pPipelineCache);

                if (palResult == Util::Result::Success)
                {
                    pPalPipeline[deviceIdx]->Destroy();

                    palResult = pPalDevice->CreateGraphicsPipeline(
                        pObjectCreateInfo->pipeline,
                        Util::VoidPtrInc(pSystemMem, palOffset),
                        &pPalPipeline[deviceIdx]);
                }
                else if (palResult == Util::Result::NotFound)
                {
                    // If a replacement was not found, proceed with the original
                    palResult = Util::Result::Success;
                }
            }

            VK_ASSERT(palSize == pPalDevice->GetGraphicsPipelineSize(pObjectCreateInfo->pipeline, nullptr));
            palOffset += palSize;
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Create graphics pipeline objects
VkResult GraphicsPipeline::CreatePipelineObjects(
    Device*                             pDevice,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    VkPipelineCreateFlags2KHR           flags,
    const VkAllocationCallbacks*        pAllocator,
    const UserDataLayout*               pLayout,
    const VbBindingInfo*                pVbInfo,
    const PipelineInternalBufferInfo*   pInternalBuffer,
    const InternalMemory*               pInternalMem,
    const Vkgc::BinaryData*             pPipelineBinaries,
    PipelineCache*                      pPipelineCache,
    const Util::MetroHash::Hash*        pCacheIds,
    uint64_t                            apiPsoHash,
    const PipelineBinaryStorage&        binaryStorage,
    GraphicsPipelineObjectCreateInfo*   pObjectCreateInfo,
    VkPipeline*                         pPipeline)
{
    VkResult          result        = VK_SUCCESS;
    Pal::Result       palResult     = Pal::Result::Success;
    const uint32_t    numPalDevices = pDevice->NumPalDevices();
    Util::MetroHash64 palPipelineHasher;

    palPipelineHasher.Update(pObjectCreateInfo->pipeline);

    // Create the PAL pipeline object.
    Pal::IPipeline*          pPalPipeline[MaxPalDevices]     = {};
    Pal::IMsaaState*         pPalMsaa[MaxPalDevices]         = {};
    Pal::IColorBlendState*   pPalColorBlend[MaxPalDevices]   = {};
    Pal::IDepthStencilState* pPalDepthStencil[MaxPalDevices] = {};

    // Get the pipeline size from PAL and allocate memory.
    void*  pSystemMem = nullptr;
    size_t palSize    = 0;
    pObjectCreateInfo->pipeline.pipelineBinarySize = pPipelineBinaries[DefaultDeviceIndex].codeSize;
    pObjectCreateInfo->pipeline.pPipelineBinary    = pPipelineBinaries[DefaultDeviceIndex].pCode;

    palSize =
        pDevice->PalDevice(DefaultDeviceIndex)->GetGraphicsPipelineSize(pObjectCreateInfo->pipeline, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    size_t     allocationSize        = sizeof(GraphicsPipeline) + (palSize * numPalDevices);
    const bool storeBinaryToPipeline = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) != 0;

    if (storeBinaryToPipeline)
    {
        allocationSize += sizeof(PipelineBinaryStorage);
    }

    pSystemMem = pDevice->AllocApiObject(
        pAllocator,
        allocationSize);

    if (pSystemMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    RenderStateCache* pRSCache = pDevice->GetRenderStateCache();

    if (result == VK_SUCCESS)
    {
        result = CreatePalPipelineObjects(pDevice,
            pPipelineCache,
            pObjectCreateInfo,
            pPipelineBinaries,
            pCacheIds,
            Util::VoidPtrInc(pSystemMem, sizeof(GraphicsPipeline)),
            pPalPipeline);
    }

    PipelineBinaryStorage* pPermBinaryStorage = nullptr;

    if (result == VK_SUCCESS)
    {
        bool sampleShadingEnable = pObjectCreateInfo->flags.sampleShadingEnable;
        for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

            // Create the PAL MSAA state object
            if (palResult == Pal::Result::Success)
            {
                // Force full sample shading if the app didn't enable it, but the shader wants
                // per-sample shading by the use of SampleId or similar features.
                if ((pObjectCreateInfo->immedInfo.rasterizerDiscardEnable != VK_TRUE) &&
                    (sampleShadingEnable == false))
                {
                    const auto& palProperties = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();
                    const auto& info          = pPalPipeline[deviceIdx]->GetInfo();

                    if (info.ps.flags.perSampleShading == 1)
                    {
                        // Override the shader rate to 1x1 if SampleId used in shader.
                        Device::SetDefaultVrsRateParams(&pObjectCreateInfo->immedInfo.vrsRateParams);

                        pObjectCreateInfo->flags.force1x1ShaderRate = true;
                        if (pObjectCreateInfo->flags.bindMsaaObject == false)
                        {
                            pObjectCreateInfo->flags.sampleShadingEnable = true;
                            pObjectCreateInfo->immedInfo.minSampleShading = 1.0f;
                        }

                        pObjectCreateInfo->immedInfo.msaaCreateInfo.pixelShaderSamples =
                            pObjectCreateInfo->immedInfo.msaaCreateInfo.coverageSamples;

                        SetVrsCombinerStagePsIterSamples(
                            pObjectCreateInfo->immedInfo.msaaCreateInfo.pixelShaderSamples,
                            pObjectCreateInfo->immedInfo.vrsRateParams.flags.exposeVrsPixelsMask,
                            pObjectCreateInfo->immedInfo.vrsRateParams.combinerState);
                    }
                    else if ((info.ps.flags.usesSampleMask == 1) &&
                             (palProperties.gfxipProperties.flags.supportVrsWithDsExports == 0))
                    {
                        // Override the shader rate to 1x1 if supportVrsWithDsExports is not supported and SampleMask
                        // used in shader.
                        Device::SetDefaultVrsRateParams(&pObjectCreateInfo->immedInfo.vrsRateParams);

                        pObjectCreateInfo->flags.force1x1ShaderRate = true;
                        pObjectCreateInfo->immedInfo.msaaCreateInfo.pixelShaderSamples = 1;
                    }
                    else if (info.ps.flags.enablePops == 1)
                    {
                        // Override the shader rate to 1x1 if POPS is enabled and
                        // fragmentShadingRateWithFragmentShaderInterlock is not supported.
                        Device::SetDefaultVrsRateParams(&pObjectCreateInfo->immedInfo.vrsRateParams);

                        pObjectCreateInfo->flags.force1x1ShaderRate = true;
                    }
                }

                if (pObjectCreateInfo->flags.bindMsaaObject)
                {
                    palResult = pRSCache->CreateMsaaState(
                        pObjectCreateInfo->immedInfo.msaaCreateInfo,
                        pAllocator,
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                        pPalMsaa);
                }
            }

            // Create the PAL color blend state object
            if ((palResult == Pal::Result::Success) && pObjectCreateInfo->flags.bindColorBlendObject)
            {
                palResult = pRSCache->CreateColorBlendState(
                    pObjectCreateInfo->immedInfo.blendCreateInfo,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalColorBlend);
            }

            // Create the PAL depth stencil state object
            if ((palResult == Pal::Result::Success) && pObjectCreateInfo->flags.bindDepthStencilObject)
            {
                palResult = pRSCache->CreateDepthStencilState(
                    pObjectCreateInfo->immedInfo.depthStencilCreateInfo,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalDepthStencil);
            }

            // Reset the PAL stencil maskupdate flags
            pObjectCreateInfo->immedInfo.stencilRefMasks.flags.u8All = 0xff;
        }

        result = PalToVkResult(palResult);

        if ((result == VK_SUCCESS) && storeBinaryToPipeline)
        {
            size_t pipelineBinaryOffset = sizeof(GraphicsPipeline) + (palSize * numPalDevices);
            pPermBinaryStorage = static_cast<PipelineBinaryStorage*>(Util::VoidPtrInc(pSystemMem,
                                                                                      pipelineBinaryOffset));

            // Simply copy the existing allocations to the new struct.
            memcpy(pPermBinaryStorage, &binaryStorage, sizeof(PipelineBinaryStorage));
        }
    }

    // On success, wrap it up in a Vulkan object.
    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) GraphicsPipeline(
            pDevice,
            pPalPipeline,
            pLayout,
            pPermBinaryStorage,
            pObjectCreateInfo->immedInfo,
            pObjectCreateInfo->staticStateMask,
            pObjectCreateInfo->flags,
#if VKI_RAY_TRACING
            pObjectCreateInfo->dispatchRaysUserDataOffset,
#endif
            *pVbInfo,
            pInternalBuffer,
            pPalMsaa,
            pPalColorBlend,
            pPalDepthStencil,
            pObjectCreateInfo->pipeline.ppShaderLibraries,
            pInternalMem,
            pObjectCreateInfo->sampleCoverage,
            pCacheIds[DefaultDeviceIndex],
            apiPsoHash,
            &palPipelineHasher);

        *pPipeline = GraphicsPipeline::HandleFromVoidPointer(pSystemMem);
        if (pDevice->GetEnabledFeatures().enableDebugPrintf)
        {
            GraphicsPipeline* pGraphicsPipeline = static_cast<GraphicsPipeline*>(pSystemMem);
            pGraphicsPipeline->ClearFormatString();
            DebugPrintf::DecodeFormatStringsFromElf(
                pDevice,
                pPipelineBinaries[DefaultDeviceIndex].codeSize,
                static_cast<const char*>(pPipelineBinaries[DefaultDeviceIndex].pCode),
                pGraphicsPipeline->GetFormatStrings());
        }
    }

    if (result != VK_SUCCESS)
    {
        if (pPalMsaa[0] != nullptr)
        {
            pRSCache->DestroyMsaaState(pPalMsaa, pAllocator);
        }

        if (pPalColorBlend[0] != nullptr)
        {
            pRSCache->DestroyColorBlendState(pPalColorBlend, pAllocator);
        }

        if (pPalDepthStencil[0] != nullptr)
        {
            pRSCache->DestroyDepthStencilState(pPalDepthStencil, pAllocator);
        }

        // Something went wrong with creating the PAL object. Free memory and return error.
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            if (pPalPipeline[deviceIdx] != nullptr)
            {
                pPalPipeline[deviceIdx]->Destroy();
            }
        }

        pDevice->FreeApiObject(pAllocator, pSystemMem);
    }

    return result;
}

// =====================================================================================================================
static bool IsGplFastLinkPossible(
    const Device*                      pDevice,
    const GraphicsPipelineLibraryInfo& libInfo,
    const UserDataLayout*              pUserDataLayout)
{
    bool result = false;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    if ((libInfo.flags.isLibrary == false) &&
        (libInfo.flags.optimize == false) &&
        (libInfo.pFragmentShaderLib != nullptr) &&
        (libInfo.pPreRasterizationShaderLib != nullptr))
    {
        const GraphicsPipelineBinaryCreateInfo& preRasterCreateInfo =
            libInfo.pPreRasterizationShaderLib->GetPipelineBinaryCreateInfo();
        const GraphicsPipelineBinaryCreateInfo& fragmentCreateInfo =
            libInfo.pFragmentShaderLib->GetPipelineBinaryCreateInfo();

        bool isPushConstCompatible = true;

        if (settings.pipelineLayoutPushConstantCompatibilityCheck)
        {
            isPushConstCompatible = (pUserDataLayout->common.pushConstRegCount ==
                                     libInfo.pFragmentShaderLib->GetUserDataLayout()->common.pushConstRegCount) &&
                                    (pUserDataLayout->common.pushConstRegCount ==
                                     libInfo.pPreRasterizationShaderLib->GetUserDataLayout()->common.pushConstRegCount);
        }

        if ((preRasterCreateInfo.pShaderLibraries[GraphicsLibraryPreRaster] != nullptr) &&
            (fragmentCreateInfo.pShaderLibraries[GraphicsLibraryFragment] != nullptr) &&
            isPushConstCompatible)
        {
            result = true;
        }
    }

    return result;
}

// =====================================================================================================================
void DumpGplFastLinkInfo(
    const Device*                           pDevice,
    VkPipeline                              pipeline,
    const GraphicsPipelineBinaryCreateInfo& createInfo,
    const GraphicsPipelineLibraryInfo&      libInfo)
{
    const GraphicsPipeline* pGraphicsPipeline = GraphicsPipeline::ObjectFromHandle(pipeline);
    const Pal::IPipeline*   pPalPipeline      = pGraphicsPipeline->GetPalPipeline(DefaultDeviceIndex);
    const Pal::PipelineInfo info              = pPalPipeline->GetInfo();
    const RuntimeSettings&  settings          = pDevice->GetRuntimeSettings();

    uint64_t dumpHash = settings.dumpPipelineWithApiHash ? createInfo.apiPsoHash : info.internalPipelineHash.stable;

    Vkgc::PipelineDumpOptions dumpOptions = {};
    char tempBuff[Util::MaxPathStrLen];
    PipelineCompiler::InitPipelineDumpOption(&dumpOptions, settings, tempBuff, createInfo.compilerType);

    Vkgc::PipelineBuildInfo pipelineInfo = {};
    pipelineInfo.pGraphicsInfo = &createInfo.pipelineInfo;

    void* pPipelineDumpHandle = Vkgc::IPipelineDumper::BeginPipelineDump(&dumpOptions, pipelineInfo, dumpHash);
    if (pPipelineDumpHandle != nullptr)
    {
        char preRasterFileName[Util::MaxFileNameStrLen] = {};
        char fragmentFileName[Util::MaxFileNameStrLen] = {};
        char colorExportFileName[Util::MaxFileNameStrLen] = {};

        const GraphicsPipelineBinaryCreateInfo& preRasterCreateInfo =
            libInfo.pPreRasterizationShaderLib->GetPipelineBinaryCreateInfo();
        const GraphicsPipelineBinaryCreateInfo& fragmentCreateInfo =
            libInfo.pFragmentShaderLib->GetPipelineBinaryCreateInfo();

        uint64_t preRasterHash = settings.dumpPipelineWithApiHash ?
            preRasterCreateInfo.apiPsoHash : preRasterCreateInfo.libraryHash[GraphicsLibraryPreRaster];
        uint64_t fragmentHash = settings.dumpPipelineWithApiHash ?
            fragmentCreateInfo.apiPsoHash : fragmentCreateInfo.libraryHash[GraphicsLibraryFragment];

        Vkgc::IPipelineDumper::GetPipelineName(&preRasterCreateInfo.pipelineInfo,
            preRasterFileName, Util::MaxFileNameStrLen, preRasterHash);
        Vkgc::IPipelineDumper::GetPipelineName(&fragmentCreateInfo.pipelineInfo,
            fragmentFileName, Util::MaxFileNameStrLen, fragmentHash);

        if (createInfo.pipelineInfo.enableColorExportShader)
        {
            uint64_t colorExportHash = settings.dumpPipelineWithApiHash ?
                createInfo.apiPsoHash : createInfo.libraryHash[GraphicsLibraryColorExport];

            Vkgc::GraphicsPipelineBuildInfo colorExportInfo = {};
            colorExportInfo.unlinked = true;
            Vkgc::IPipelineDumper::GetPipelineName(&colorExportInfo,
                colorExportFileName, Util::MaxFileNameStrLen, colorExportHash);
        }

        const char* fileNames[] = {preRasterFileName, fragmentFileName, colorExportFileName};
        Vkgc::IPipelineDumper::DumpGraphicsLibraryFileName(pPipelineDumpHandle, fileNames);

        char extraInfo[256] = {};
        Util::Snprintf(
            extraInfo,
            sizeof(extraInfo),
            "\n; ApiPsoHash: 0x%016" PRIX64 "\n",
            createInfo.apiPsoHash);
        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, extraInfo);

        for (uint32_t i = 0; i < GraphicsLibraryCount; i++)
        {
            if (createInfo.pShaderLibraries[i] == nullptr)
            {
                continue;
            }
            uint32_t codeSize = 0;
            Pal::Result result = createInfo.pShaderLibraries[i]->GetCodeObject(&codeSize, nullptr);
            if ((codeSize > 0) && (result == Pal::Result::Success))
            {
                void* pCode = pDevice->VkInstance()->AllocMem(codeSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
                if (pCode != nullptr)
                {
                    result = createInfo.pShaderLibraries[i]->GetCodeObject(&codeSize, pCode);
                    VK_ASSERT(result == Pal::Result::Success);

                    Vkgc::BinaryData libraryBinary = {};
                    libraryBinary.codeSize = codeSize;
                    libraryBinary.pCode    = pCode;
                    Vkgc::IPipelineDumper::DumpPipelineBinary(
                        pPipelineDumpHandle, pDevice->GetCompiler(DefaultDeviceIndex)->GetGfxIp(), &libraryBinary);

                    pDevice->VkInstance()->FreeMem(pCode);
                }
            }
        }

        PipelineCompiler::DumpPipelineMetadata(pPipelineDumpHandle, createInfo.pBinaryMetadata);

        Vkgc::IPipelineDumper::DumpPipelineExtraInfo(pPipelineDumpHandle, "\n;CompileResult=FastLinkSuccess\n");
        Vkgc::IPipelineDumper::EndPipelineDump(pPipelineDumpHandle);
    }
}

// =====================================================================================================================
// Create a graphics pipeline object.
VkResult GraphicsPipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineExtStructs&       extStructs,
    VkPipelineCreateFlags2KHR               flags,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    VkResult result = VK_SUCCESS;
    uint64 startTimeTicks = Util::GetPerfCpuTime();
    uint64_t colorExportDuration = 0;

    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    auto pPipelineCreationFeedbackCreateInfo = extStructs.pPipelineCreationFeedbackCreateInfoEXT;

    PipelineCompiler::InitPipelineCreationFeedback(pPipelineCreationFeedbackCreateInfo);

    GraphicsPipelineBinaryCreateInfo binaryCreateInfo     = {};
    GraphicsPipelineObjectCreateInfo objectCreateInfo     = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo      = {};
    PipelineOptimizerKey             pipelineOptimizerKey = {};
    uint64_t                         apiPsoHash           = 0;
    Util::MetroHash::Hash            elfHash              = {};
    PipelineMetadata                 binaryMetadata       = {};
    bool                             enableFastLink       = false;
    bool                             binariesProvided     = false;
    bool                             gplProvided          = false;
    uint32                           gplMask              = 0;
    ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount]         = {};
    ShaderOptimizerKey               shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
    Vkgc::BinaryData                 pipelineBinaries[MaxPalDevices]                       = {};
    Util::MetroHash::Hash            cacheId[MaxPalDevices]                                = {};
    const Pal::IShaderLibrary*       shaderLibraries[GraphicsLibraryCount]                 = {};
    Util::MetroHash::Hash            gplCacheId[GraphicsLibraryCount]                      = {};
    uint32_t                         numShaderLibraries = 0;
    PipelineResourceLayout           resourceLayout     = {};

    auto pPipelineBinaryInfoKHR = extStructs.pPipelineBinaryInfoKHR;

    if (pPipelineBinaryInfoKHR != nullptr)
    {
        if (pPipelineBinaryInfoKHR->binaryCount > 0)
        {
            VK_ASSERT(pPipelineBinaryInfoKHR->binaryCount == pDevice->NumPalDevices());
            binariesProvided = true;
        }

        for (uint32_t deviceIdx = 0;
             deviceIdx < pPipelineBinaryInfoKHR->binaryCount;
             ++deviceIdx)
        {
            const auto pBinary = PipelineBinary::ObjectFromHandle(
                pPipelineBinaryInfoKHR->pPipelineBinaries[deviceIdx]);

            cacheId[deviceIdx]          = pBinary->BinaryKey();
            pipelineBinaries[deviceIdx] = pBinary->BinaryData();

            if (deviceIdx == DefaultDeviceIndex)
            {
                pDefaultCompiler->ReadBinaryMetadata(
                    pDevice,
                    pipelineBinaries[deviceIdx],
                    &binaryMetadata);
            }
        }
    }

    PipelineBinaryStorage binaryStorage         = {};
    const bool            storeBinaryToPipeline = (flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) != 0;

    BuildPipelineResourceLayout(pDevice,
                               PipelineLayout::ObjectFromHandle(pCreateInfo->layout),
                               VK_PIPELINE_BIND_POINT_GRAPHICS,
                               flags,
                               &resourceLayout);

    GraphicsPipelineLibraryInfo libInfo = {};
    GraphicsPipelineCommon::ExtractLibraryInfo(pDevice, pCreateInfo, extStructs, flags, &libInfo);

    if (libInfo.pPreRasterizationShaderLib != nullptr)
    {
        gplMask     |= (1 << GraphicsLibraryPreRaster);
        gplProvided = true;
    }
    if (libInfo.pFragmentShaderLib != nullptr)
    {
        gplMask     |= (1 << GraphicsLibraryFragment);
        gplProvided = true;
    }

    // If this triggers, then the pPipelineBinaryInfoKHR handling code will need to handle the case where the binaries
    // that are provided are GPL binaries, not monolithic binaries.
    VK_ASSERT((gplProvided && binariesProvided) == false);

    // 1. Check whether GPL fast link is possible
    // If pipeline only contains PreRasterizationShaderLib and no fragment shader is in the create info,
    // we add a null fragment library in order to use fast link.
    if ((libInfo.flags.isLibrary == false) &&
        ((libInfo.pPreRasterizationShaderLib != nullptr) && (libInfo.pFragmentShaderLib == nullptr)))
    {
        bool hasFragShader = false;
        for (uint32_t i = 0; i < pCreateInfo->stageCount; ++i)
        {
            if (ShaderFlagBitToStage(pCreateInfo->pStages[i].stage) == ShaderStageFragment)
            {
                hasFragShader = true;
                break;
            }
        }

        if (hasFragShader == false)
        {
            libInfo.pFragmentShaderLib = pDevice->GetNullFragmentLib();
        }
    }

    if (IsGplFastLinkPossible(pDevice, libInfo, &resourceLayout.userDataLayout) && (result == VK_SUCCESS))
    {
        result = pDefaultCompiler->BuildGplFastLinkCreateInfo(pDevice,
                                                              pCreateInfo,
                                                              extStructs,
                                                              flags,
                                                              libInfo,
                                                              &resourceLayout.userDataLayout,
                                                              &binaryMetadata,
                                                              &binaryCreateInfo);

        if (result == VK_SUCCESS)
        {
            const GraphicsPipelineBinaryCreateInfo& preRasterCreateInfo =
                libInfo.pPreRasterizationShaderLib->GetPipelineBinaryCreateInfo();
            const GraphicsPipelineBinaryCreateInfo& fragmentCreateInfo =
                libInfo.pFragmentShaderLib->GetPipelineBinaryCreateInfo();

            shaderLibraries[numShaderLibraries++] = preRasterCreateInfo.pShaderLibraries[GraphicsLibraryPreRaster];
            shaderLibraries[numShaderLibraries++] = fragmentCreateInfo.pShaderLibraries[GraphicsLibraryFragment];

            gplCacheId[GraphicsLibraryPreRaster] =
                preRasterCreateInfo.earlyElfPackageHash[GraphicsLibraryPreRaster];
            gplCacheId[GraphicsLibraryFragment]  =
                fragmentCreateInfo.earlyElfPackageHash[GraphicsLibraryFragment];

            if (binaryCreateInfo.pipelineInfo.enableColorExportShader)
            {
                uint64_t colorExportTicks = Util::GetPerfCpuTime();
                Pal::IShaderLibrary* pColorExportLib = nullptr;
                result = pDefaultCompiler->CreateColorExportShaderLibrary(pDevice,
                    &binaryCreateInfo,
                    pAllocator,
                    &pColorExportLib);
                if (result == VK_SUCCESS)
                {
                    shaderLibraries[numShaderLibraries++] = pColorExportLib;
                    binaryCreateInfo.pShaderLibraries[GraphicsLibraryColorExport] = pColorExportLib;
                }
                uint64_t durationTicks = Util::GetPerfCpuTime() - colorExportTicks;
                colorExportDuration = vk::utils::TicksToNano(durationTicks);
            }
        }
        else if (result == VK_ERROR_PIPELINE_COMPILE_REQUIRED_EXT)
        {
            flags |= VK_PIPELINE_CREATE_2_LINK_TIME_OPTIMIZATION_BIT_EXT;
        }

        if (result == VK_SUCCESS)
        {
            objectCreateInfo.pipeline.ppShaderLibraries = shaderLibraries;
            objectCreateInfo.pipeline.numShaderLibraries = numShaderLibraries;
            if ((pDevice->VkInstance()->GetDevModeMgr() != nullptr) ||
                pDevice->GetRuntimeSettings().enablePipelineDump ||
                pDevice->GetRuntimeSettings().logTagIdMask)
            {
                BuildApiHash(pCreateInfo,
                            flags,
                            extStructs,
                            libInfo,
                            binaryCreateInfo,
                            &apiPsoHash,
                            &elfHash);
                binaryCreateInfo.apiPsoHash = apiPsoHash;
            }
            enableFastLink = true;
        }
    }

    if ((enableFastLink == false) && (binariesProvided == false))
    {
        // 2. Create Cache IDs
        result = GraphicsPipeline::CreateCacheId(
            pDevice,
            pCreateInfo,
            extStructs,
            libInfo,
            flags,
            &shaderStageInfo,
            &binaryCreateInfo,
            &shaderOptimizerKeys[0],
            &pipelineOptimizerKey,
            &apiPsoHash,
            &elfHash,
            &tempModules[0],
            &cacheId[0]);

        binaryCreateInfo.apiPsoHash = apiPsoHash;

        // 3. Create pipeline binaries (or load from cache)
        if (result == VK_SUCCESS)
        {
            result = CreatePipelineBinaries(
                pDevice,
                pCreateInfo,
                extStructs,
                libInfo,
                flags,
                &shaderStageInfo,
                &resourceLayout,
                &pipelineOptimizerKey,
                &binaryCreateInfo,
                pPipelineCache,
                pPipelineCreationFeedbackCreateInfo,
                cacheId,
                pipelineBinaries,
                &binaryMetadata);
        }
    }

    // 4. Store created binaries for pipeline_binary
    if ((result == VK_SUCCESS) && storeBinaryToPipeline)
    {
        if (gplProvided)
        {
            const PipelineCompilerType compilerType =
                pDefaultCompiler->CheckCompilerType<Vkgc::GraphicsPipelineBuildInfo>(nullptr, 0, 0);
            CompilerSolution*          pSolution    = pDefaultCompiler->GetSolution(compilerType);
            uint32                     binaryIndex  = 0;

            for (uint32_t gplType = 0;
                 (gplType < GraphicsLibraryCount)            &&
                 (shaderLibraries[gplType] != nullptr)       &&
                 Util::TestAnyFlagSet(gplMask, 1 << gplType) &&
                 (result == VK_SUCCESS);
                 ++gplType)
            {
                Vkgc::BinaryData shaderBinary = {};
                bool             hitCache     = false;
                bool             hitAppCache  = false;

                pSolution->LoadShaderBinaryFromCache(
                    nullptr,
                    &gplCacheId[gplType],
                    &shaderBinary,
                    &hitCache,
                    &hitAppCache);

                if (hitCache || hitAppCache)
                {
                    Util::MetroHash::Hash libraryElfHash = {};

                    if (gplType == GraphicsLibraryPreRaster)
                    {
                        libraryElfHash = *libInfo.pPreRasterizationShaderLib->GetElfHash();
                    }
                    else if (gplType == GraphicsLibraryFragment)
                    {
                        libraryElfHash = *libInfo.pFragmentShaderLib->GetElfHash();
                    }

                    result = GraphicsPipelineLibrary::WriteGplAndMetadataToPipelineBinary(
                        pAllocator,
                        shaderBinary,
                        gplCacheId[gplType],
                        static_cast<GraphicsLibraryType>(gplType),
                        libraryElfHash,
                        binaryIndex,
                        &binaryStorage);

                    if (result == VK_SUCCESS)
                    {
                        ++binaryIndex;
                    }
                }
            }
        }
        else
        {
            for (uint32_t deviceIdx = 0; (deviceIdx < pDevice->NumPalDevices()) && (result == VK_SUCCESS); ++deviceIdx)
            {
                void* pMemory = pAllocator->pfnAllocation(
                    pAllocator->pUserData,
                    pipelineBinaries[deviceIdx].codeSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);     // retained in the pipeline object

                if (pMemory != nullptr)
                {
                    memcpy(
                        pMemory,
                        pipelineBinaries[deviceIdx].pCode,
                        pipelineBinaries[deviceIdx].codeSize);

                    InsertBinaryData(
                        &binaryStorage,
                        deviceIdx,
                        cacheId[deviceIdx],
                        pipelineBinaries[deviceIdx].codeSize,
                        pMemory);
                }
                else
                {
                    result = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        // 5. Build pipeline object create info
        BuildPipelineObjectCreateInfo(
            pDevice,
            pCreateInfo,
            extStructs,
            libInfo,
            flags,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &objectCreateInfo,
            &binaryCreateInfo);

        if (result == VK_SUCCESS)
        {
#if VKI_RAY_TRACING
            objectCreateInfo.flags.hasRayTracing = binaryMetadata.rayQueryUsed;
#endif
            objectCreateInfo.flags.isPointSizeUsed = binaryMetadata.pointSizeUsed;
            objectCreateInfo.flags.shadingRateUsedInShader = binaryMetadata.shadingRateUsedInShader;

            if (libInfo.pPreRasterizationShaderLib != nullptr)
            {
                if (libInfo.pPreRasterizationShaderLib->GetPipelineBinaryCreateInfo().flags &
                    VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT)
                {
                    objectCreateInfo.flags.viewIndexFromDeviceIndex |= 1 << GraphicsLibraryPreRaster;
                }
            }

            if (libInfo.pFragmentShaderLib != nullptr)
            {
                if (libInfo.pFragmentShaderLib->GetPipelineBinaryCreateInfo().flags &
                    VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT)
                {
                    objectCreateInfo.flags.viewIndexFromDeviceIndex |= 1 << GraphicsLibraryFragment;
                }
            }

            if (Util::TestAnyFlagSet(flags, VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT))
            {
                objectCreateInfo.flags.viewIndexFromDeviceIndex |=
                    ((1 << GraphicsLibraryPreRaster) | (1 << GraphicsLibraryFragment));
            }

#if VKI_RAY_TRACING
            objectCreateInfo.dispatchRaysUserDataOffset = GetDispatchRaysUserData(&resourceLayout);
#endif

            // 6. Create pipeline objects
            result = CreatePipelineObjects(
                pDevice,
                pCreateInfo,
                flags,
                pAllocator,
                &resourceLayout.userDataLayout,
                &binaryMetadata.vbInfo,
                &binaryMetadata.internalBufferInfo,
                binaryCreateInfo.pInternalMem,
                pipelineBinaries,
                pPipelineCache,
                cacheId,
                apiPsoHash,
                binaryStorage,
                &objectCreateInfo,
                pPipeline);

            if (result != VK_SUCCESS)
            {
                // Free the binaries only if we failed to create the pipeline objects.
                FreeBinaryStorage(&binaryStorage, pAllocator);
            }
        }
    }

    // Free the temporary newly-built shader modules
    FreeTempModules(pDevice, ShaderStage::ShaderStageGfxCount, tempModules);

    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if ((binariesProvided == false) && (pipelineBinaries[deviceIdx].pCode != nullptr))
        {
            pDevice->GetCompiler(deviceIdx)->FreeGraphicsPipelineBinary(
                binaryCreateInfo.compilerType,
                binaryCreateInfo.freeCompilerBinary,
                pipelineBinaries[deviceIdx]);
        }
    }

    if (result == VK_SUCCESS)
    {
        const Device::DeviceFeatures& deviceFeatures = pDevice->GetEnabledFeatures();

        uint64_t durationTicks = Util::GetPerfCpuTime() - startTimeTicks;
        uint64_t duration = vk::utils::TicksToNano(durationTicks);
        binaryCreateInfo.pipelineFeedback.feedbackValid = true;
        binaryCreateInfo.pipelineFeedback.duration = duration;
        PipelineCompiler::SetPipelineCreationFeedbackInfo(
            pPipelineCreationFeedbackCreateInfo,
            pCreateInfo->stageCount,
            pCreateInfo->pStages,
            &binaryCreateInfo.pipelineFeedback,
            binaryCreateInfo.stageFeedback);

        if (deviceFeatures.gpuMemoryEventHandler)
        {
            size_t numEntries = 0;
            Util::Vector<Pal::GpuMemSubAllocInfo, 1, PalAllocator> palSubAllocInfos(pDevice->VkInstance()->Allocator());
            const auto* pPipelineObject = GraphicsPipeline::ObjectFromHandle(*pPipeline);

            pPipelineObject->PalPipeline(DefaultDeviceIndex)->QueryAllocationInfo(&numEntries, nullptr);

            palSubAllocInfos.Resize(numEntries);

            pPipelineObject->PalPipeline(DefaultDeviceIndex)->QueryAllocationInfo(&numEntries, &palSubAllocInfos[0]);

            for (size_t i = 0; i < numEntries; ++i)
            {
                // Report the Pal suballocation for this pipeline to device_memory_report
                pDevice->VkInstance()->GetGpuMemoryEventHandler()->ReportDeferredPalSubAlloc(
                    pDevice,
                    palSubAllocInfos[i].address,
                    palSubAllocInfos[i].offset,
                    GraphicsPipeline::IntValueFromHandle(*pPipeline),
                    VK_OBJECT_TYPE_PIPELINE);
            }
        }

        // The hash is same as pipeline dump file name, we can easily analyze further.
        AmdvlkLog(pDevice->GetRuntimeSettings().logTagIdMask,
                  PipelineCompileTime,
                  "0x%016llX-IsLib: %u-Opt: %u-LibFlag: %u-Duration: %llu-ColorExportDuration: %llu",
                  apiPsoHash,
                  libInfo.flags.isLibrary,
                  libInfo.flags.optimize,
                  libInfo.libFlags,
                  duration,
                  colorExportDuration);

        if (enableFastLink && pDevice->GetRuntimeSettings().enablePipelineDump)
        {
            DumpGplFastLinkInfo(pDevice, *pPipeline, binaryCreateInfo, libInfo);
        }
    }

    // Deferred compile will reuse all object generated in PipelineCompiler::ConvertGraphicsPipelineInfo.
    // i.e. we need keep temp buffer in binaryCreateInfo
    pDefaultCompiler->FreeGraphicsPipelineCreateInfo(pDevice,
        &binaryCreateInfo,
        true);

    return result;
}

// =====================================================================================================================
static size_t GetVertexInputStructSize(
    const VkPipelineVertexInputStateCreateInfo* pVertexInput)
{
    size_t size = 0;
    size += sizeof(VkPipelineVertexInputStateCreateInfo);
    size += sizeof(VkVertexInputBindingDescription) * pVertexInput->vertexBindingDescriptionCount;
    size += sizeof(VkVertexInputAttributeDescription) * pVertexInput->vertexAttributeDescriptionCount;

    const VkPipelineVertexInputDivisorStateCreateInfoEXT* pVertexDivisor = nullptr;
    const vk::VkStructHeader* pStructHeader =
        static_cast<const vk::VkStructHeader*>(pVertexInput->pNext);
    while (pStructHeader != nullptr)
    {
        if (pStructHeader->sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT)
        {
            pVertexDivisor = reinterpret_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pStructHeader);
            break;
        }
        else
        {
            pStructHeader = pStructHeader->pNext;
        }
    }

    if (pVertexDivisor != nullptr)
    {
        size += sizeof(VkPipelineVertexInputDivisorStateCreateInfoEXT);
        size += sizeof(VkVertexInputBindingDivisorDescriptionEXT) * pVertexDivisor->vertexBindingDivisorCount;
    }

    return size;
}

// =====================================================================================================================
// Create cacheId for a graphics pipeline.
VkResult GraphicsPipeline::CreateCacheId(
    const Device*                           pDevice,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineExtStructs&       extStructs,
    const GraphicsPipelineLibraryInfo&      libInfo,
    VkPipelineCreateFlags2KHR               flags,
    GraphicsPipelineShaderStageInfo*        pShaderStageInfo,
    GraphicsPipelineBinaryCreateInfo*       pBinaryCreateInfo,
    ShaderOptimizerKey*                     pShaderOptimizerKeys,
    PipelineOptimizerKey*                   pPipelineOptimizerKey,
    uint64_t*                               pApiPsoHash,
    Util::MetroHash::Hash*                  pElfHash,
    ShaderModuleHandle*                     pTempModules,
    Util::MetroHash::Hash*                  pCacheIds)
{
    VkResult result = VK_SUCCESS;

    // 1. Build shader stage infos
    result = BuildShaderStageInfo(pDevice,
        pCreateInfo->stageCount,
        pCreateInfo->pStages,
        [](const uint32_t inputIdx, const uint32_t stageIdx)
        {
            return stageIdx;
        },
        pShaderStageInfo->stages,
        pTempModules,
        pBinaryCreateInfo->stageFeedback);

    if (result == VK_SUCCESS)
    {
        // 2. Build ShaderOptimizer pipeline key
        GeneratePipelineOptimizerKey(
            pDevice,
            pCreateInfo,
            extStructs,
            libInfo,
            flags,
            pShaderStageInfo,
            pShaderOptimizerKeys,
            pPipelineOptimizerKey);

        // 3. Build API and ELF hashes
        BuildApiHash(pCreateInfo,
                     flags,
                     extStructs,
                     libInfo,
                     *pBinaryCreateInfo,
                     pApiPsoHash,
                     pElfHash);

        // 4. Build Cache IDs
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); ++deviceIdx)
        {
            ElfHashToCacheId(
                pDevice,
                deviceIdx,
                *pElfHash,
                *pPipelineOptimizerKey,
                &pCacheIds[deviceIdx]
            );
        }
    }

    return result;
}

// =====================================================================================================================
static void CopyVertexInputStruct(
    const VkPipelineVertexInputStateCreateInfo* pSrcVertexInput,
    VkPipelineVertexInputStateCreateInfo*       pDestVertexInput)
{
    // Copy VkPipelineVertexInputStateCreateInfo
    *pDestVertexInput = *pSrcVertexInput;
    void* pNext = Util::VoidPtrInc(pDestVertexInput, sizeof(VkPipelineVertexInputStateCreateInfo));

    // Copy VkVertexInputBindingDescription
    pDestVertexInput->pVertexBindingDescriptions = reinterpret_cast<VkVertexInputBindingDescription*>(pNext);
    size_t size = sizeof(VkVertexInputBindingDescription) * pSrcVertexInput->vertexBindingDescriptionCount;
    memcpy(pNext, pSrcVertexInput->pVertexBindingDescriptions, size);
    pNext = Util::VoidPtrInc(pNext, size);

    // Copy VkVertexInputAttributeDescription
    pDestVertexInput->pVertexAttributeDescriptions = reinterpret_cast<VkVertexInputAttributeDescription*>(pNext);
    size = sizeof(VkVertexInputAttributeDescription) * pSrcVertexInput->vertexAttributeDescriptionCount;
    memcpy(pNext, pSrcVertexInput->pVertexAttributeDescriptions, size);
    pNext = Util::VoidPtrInc(pNext, size);

    const VkPipelineVertexInputDivisorStateCreateInfoEXT* pSrcVertexDivisor = nullptr;
    const vk::VkStructHeader* pStructHeader =
        reinterpret_cast<const vk::VkStructHeader*>(pSrcVertexInput->pNext);
    while (pStructHeader != nullptr)
    {
        if (pStructHeader->sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT)
        {
            pSrcVertexDivisor = reinterpret_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pStructHeader);
            break;
        }
        else
        {
            pStructHeader = pStructHeader->pNext;
        }
    }

    if (pSrcVertexDivisor != nullptr)
    {
        // Copy VkPipelineVertexInputDivisorStateCreateInfoEXT
        VkPipelineVertexInputDivisorStateCreateInfoEXT* pDestVertexDivisor =
            reinterpret_cast<VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pNext);
        *pDestVertexDivisor = *pSrcVertexDivisor;
        pDestVertexInput->pNext = pDestVertexDivisor;
        pNext = Util::VoidPtrInc(pNext, sizeof(VkPipelineVertexInputDivisorStateCreateInfoEXT));

        // Copy VkVertexInputBindingDivisorDescriptionEXT
        pDestVertexDivisor->pVertexBindingDivisors = reinterpret_cast<VkVertexInputBindingDivisorDescriptionEXT*>(pNext);
        size = sizeof(VkVertexInputBindingDivisorDescriptionEXT) * pSrcVertexDivisor->vertexBindingDivisorCount;
        memcpy(pNext, pSrcVertexDivisor->pVertexBindingDivisors, size);
        pNext = Util::VoidPtrInc(pNext, size);
    }
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* const                          pDevice,
    Pal::IPipeline**                       pPalPipeline,
    const UserDataLayout*                  pLayout,
    PipelineBinaryStorage*                 pBinaryStorage,
    const GraphicsPipelineObjectImmedInfo& immedInfo,
    uint64_t                               staticStateMask,
    GraphicsPipelineObjectFlags            flags,
#if VKI_RAY_TRACING
    uint32_t                               dispatchRaysUserDataOffset,
#endif
    const VbBindingInfo&                   vbInfo,
    const PipelineInternalBufferInfo*      pInternalBuffer,
    Pal::IMsaaState**                      pPalMsaa,
    Pal::IColorBlendState**                pPalColorBlend,
    Pal::IDepthStencilState**              pPalDepthStencil,
    const Pal::IShaderLibrary**            pPalShaderLibrary,
    const InternalMemory*                  pInternalMem,
    uint32_t                               coverageSamples,
    const Util::MetroHash::Hash&           cacheHash,
    uint64_t                               apiHash,
    Util::MetroHash64*                     pPalPipelineHasher)
    :
    GraphicsPipelineCommon(
#if VKI_RAY_TRACING
        flags.hasRayTracing,
#endif
        pDevice),
    m_info(immedInfo),
    m_pPalShaderLibrary{},
    m_pInternalMem(pInternalMem),
    m_vbInfo(vbInfo),
    m_internalBufferInfo(*pInternalBuffer),
    m_flags(flags)
{
    Pipeline::Init(
        pPalPipeline,
        pLayout,
        pBinaryStorage,
        staticStateMask,
#if VKI_RAY_TRACING
        dispatchRaysUserDataOffset,
#endif
        cacheHash,
        apiHash);

    memcpy(m_pPalMsaa,         pPalMsaa,         sizeof(pPalMsaa[0])         * pDevice->NumPalDevices());
    memcpy(m_pPalColorBlend,   pPalColorBlend,   sizeof(pPalColorBlend[0])   * pDevice->NumPalDevices());
    memcpy(m_pPalDepthStencil, pPalDepthStencil, sizeof(pPalDepthStencil[0]) * pDevice->NumPalDevices());
    if (pPalShaderLibrary != nullptr)
    {
        memcpy(m_pPalShaderLibrary, pPalShaderLibrary, sizeof(m_pPalShaderLibrary));
    }

    CreateStaticState();

    if (ContainsDynamicState(DynamicStatesInternal::RasterizerDiscardEnable))
    {
        m_info.dynamicGraphicsState.enable.rasterizerDiscardEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::ColorWriteEnable) ||
        ContainsDynamicState(DynamicStatesInternal::ColorWriteMask))
    {
        m_info.dynamicGraphicsState.enable.colorWriteMask = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::LogicOp) ||
        ContainsDynamicState(DynamicStatesInternal::LogicOpEnable))
    {
        m_info.dynamicGraphicsState.enable.logicOp = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::LineRasterizationMode))
    {
        m_info.dynamicGraphicsState.enable.perpLineEndCapsEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::TessellationDomainOrigin))
    {
        m_info.dynamicGraphicsState.enable.switchWinding = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::AlphaToCoverageEnable))
    {
        m_info.dynamicGraphicsState.enable.alphaToCoverageEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClipNegativeOneToOne))
    {
        m_info.dynamicGraphicsState.enable.depthRange = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClipEnable))
    {
        m_info.dynamicGraphicsState.enable.depthClipMode = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClampEnable))
    {
        m_info.dynamicGraphicsState.enable.depthClampMode = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::ColorBlendEquation))
    {
        m_info.dynamicGraphicsState.enable.dualSourceBlendEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::VertexInput))
    {
        m_info.dynamicGraphicsState.enable.vertexBufferCount = 1;
    }

    pPalPipelineHasher->Update(m_palPipelineHash);
    pPalPipelineHasher->Finalize(reinterpret_cast<uint8*>(&m_palPipelineHash));
}

// =====================================================================================================================
// Creates instances of static pipeline state.  Much of this information can be cached at the device-level to help speed
// up pipeline-bind operations.
void GraphicsPipeline::CreateStaticState()
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();
    auto* pStaticTokens      = &m_info.staticTokens;

    pStaticTokens->inputAssemblyState         = DynamicRenderStateToken;
    pStaticTokens->triangleRasterState        = DynamicRenderStateToken;
    pStaticTokens->pointLineRasterState       = DynamicRenderStateToken;
    pStaticTokens->depthBias                  = DynamicRenderStateToken;
    pStaticTokens->blendConst                 = DynamicRenderStateToken;
    pStaticTokens->depthBounds                = DynamicRenderStateToken;
    pStaticTokens->viewport                   = DynamicRenderStateToken;
    pStaticTokens->scissorRect                = DynamicRenderStateToken;
    pStaticTokens->lineStippleState           = DynamicRenderStateToken;

    pStaticTokens->fragmentShadingRate        = DynamicRenderStateToken;

    if (m_flags.bindInputAssemblyState)
    {
        pStaticTokens->inputAssemblyState = pCache->CreateInputAssemblyState(m_info.inputAssemblyState);
    }

    if (m_flags.bindTriangleRasterState)
    {
        pStaticTokens->triangleRasterState = pCache->CreateTriangleRasterState(m_info.triangleRasterState);
    }

    if (ContainsStaticState(DynamicStatesInternal::LineWidth))
    {
        pStaticTokens->pointLineRasterState = pCache->CreatePointLineRasterState(
            m_info.pointLineRasterParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::DepthBias))
    {
        pStaticTokens->depthBias = pCache->CreateDepthBias(m_info.depthBiasParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::BlendConstants))
    {
        pStaticTokens->blendConst = pCache->CreateBlendConst(m_info.blendConstParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::DepthBounds))
    {
        pStaticTokens->depthBounds = pCache->CreateDepthBounds(m_info.depthBoundParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::Viewport) &&
        ContainsStaticState(DynamicStatesInternal::DepthClipNegativeOneToOne))
    {
        pStaticTokens->viewport = pCache->CreateViewport(m_info.viewportParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::Scissor))
    {
        pStaticTokens->scissorRect = pCache->CreateScissorRect(m_info.scissorRectParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::LineStipple))
    {
        pStaticTokens->lineStippleState = pCache->CreateLineStipple(m_info.lineStippleParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::FragmentShadingRateStateKhr) && m_flags.fragmentShadingRateEnable)
    {
        pStaticTokens->fragmentShadingRate =
            pCache->CreateFragmentShadingRate(m_info.vrsRateParams);
    }
}

// =====================================================================================================================
// Destroys static pipeline state.
void GraphicsPipeline::DestroyStaticState(
    const VkAllocationCallbacks* pAllocator)
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();

    pCache->DestroyMsaaState(m_pPalMsaa, pAllocator);
    pCache->DestroyColorBlendState(m_pPalColorBlend, pAllocator);

    if (m_pPalDepthStencil[0] != nullptr)
    {
        pCache->DestroyDepthStencilState(m_pPalDepthStencil, pAllocator);
    }

    if (m_flags.bindInputAssemblyState)
    {
        pCache->DestroyInputAssemblyState(m_info.inputAssemblyState,
                                          m_info.staticTokens.inputAssemblyState);
    }

    if (m_flags.bindTriangleRasterState)
    {
        pCache->DestroyTriangleRasterState(m_info.triangleRasterState,
                                           m_info.staticTokens.triangleRasterState);
    }

    pCache->DestroyPointLineRasterState(m_info.pointLineRasterParams,
                                        m_info.staticTokens.pointLineRasterState);

    pCache->DestroyDepthBias(m_info.depthBiasParams,
                             m_info.staticTokens.depthBias);

    pCache->DestroyBlendConst(m_info.blendConstParams,
                              m_info.staticTokens.blendConst);

    pCache->DestroyDepthBounds(m_info.depthBoundParams,
                               m_info.staticTokens.depthBounds);

    pCache->DestroyViewport(m_info.viewportParams,
                            m_info.staticTokens.viewport);

    pCache->DestroyScissorRect(m_info.scissorRectParams,
                               m_info.staticTokens.scissorRect);

    pCache->DestroyLineStipple(m_info.lineStippleParams,
                               m_info.staticTokens.lineStippleState);

    pCache->DestroyFragmentShadingRate(m_info.vrsRateParams,
                                       m_info.staticTokens.fragmentShadingRate);
}

// =====================================================================================================================
VkResult GraphicsPipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    DestroyStaticState(pAllocator);

    if (m_pInternalMem != nullptr)
    {
        pDevice->MemMgr()->FreeGpuMem(m_pInternalMem);
        Util::Destructor(m_pInternalMem);
        pDevice->VkInstance()->FreeMem(const_cast<InternalMemory*>(m_pInternalMem));
    }

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
// Binds this graphics pipeline's state to the given command buffer (with passed in wavelimits)
void GraphicsPipeline::BindToCmdBuffer(
    CmdBuffer*                             pCmdBuffer
    ) const
{
    AllGpuRenderState*               pRenderState         = pCmdBuffer->RenderState();
    const Pal::DynamicGraphicsState& dynamicGraphicsState = m_info.dynamicGraphicsState;
    Pal::DynamicGraphicsState*       pGfxDynamicBindInfo  =
        &pRenderState->pipelineState[PipelineBindGraphics].dynamicBindInfo.gfxDynState;

    // Get this pipeline's static tokens
    const auto& newTokens = m_info.staticTokens;

    // Get the old static tokens.  Copy these by value because in MGPU cases we update the new token state in a loop.
    const auto oldTokens = pRenderState->staticTokens;

    // Program static pipeline state.

    // This code will attempt to skip programming state state based on redundant value checks.  These checks are often
    // represented as token compares, where the tokens are two perfect hashes of previously compiled pipelines' static
    // parameter values.
    // If VIEWPORT is static, VIEWPORT_COUNT must be static as well
    if (ContainsStaticState(DynamicStatesInternal::Viewport) &&
        ContainsStaticState(DynamicStatesInternal::DepthClipNegativeOneToOne))
    {
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.viewports, newTokens.viewport))
        {
            pCmdBuffer->SetAllViewports(m_info.viewportParams, newTokens.viewport);
        }
    }
    else
    {
        if (ContainsStaticState(DynamicStatesInternal::Viewport))
        {
            auto pViewports = pCmdBuffer->PerGpuState(0)->viewport.viewports;
            if (memcmp(pViewports,
                       m_info.viewportParams.viewports,
                       sizeof(Pal::Viewport) * m_info.viewportParams.count) != 0)
            {
                utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
                do
                {
                    for (uint32_t i = 0; i < m_info.viewportParams.count; ++i)
                    {
                        pCmdBuffer->PerGpuState(deviceGroup.Index())->viewport.viewports[i] =
                            m_info.viewportParams.viewports[i];
                    }
                } while (deviceGroup.IterateNext());
                pRenderState->dirtyGraphics.viewport = 1;
            }
        }

        if (ContainsStaticState(DynamicStatesInternal::ViewportCount))
        {
            utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
            do
            {
                uint32* pViewportCount = &(pCmdBuffer->PerGpuState(deviceGroup.Index())->viewport.count);

                if (*pViewportCount != m_info.viewportParams.count)
                {
                    *pViewportCount = m_info.viewportParams.count;
                    pRenderState->dirtyGraphics.viewport = 1;
                }
            } while (deviceGroup.IterateNext());
        }

        Pal::DepthRange depthRange = pGfxDynamicBindInfo->depthRange;
        if (ContainsStaticState(DynamicStatesInternal::DepthClipNegativeOneToOne))
        {
            depthRange = m_info.viewportParams.depthRange;
        }

        // We can just check DefaultDeviceIndex as the value can't vary between GPUs.
        if (pCmdBuffer->PerGpuState(DefaultDeviceIndex)->viewport.depthRange != depthRange)
        {
            utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
            do
            {
                pCmdBuffer->PerGpuState(deviceGroup.Index())->viewport.depthRange = depthRange;
            } while (deviceGroup.IterateNext());

            pRenderState->dirtyGraphics.viewport = 1;
        }
    }

    if (ContainsStaticState(DynamicStatesInternal::DepthClampControl))
    {
        pRenderState->depthClampOverride = m_info.depthClampOverride;
    }

    if (ContainsStaticState(DynamicStatesInternal::Scissor))
    {
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.scissorRect, newTokens.scissorRect))
        {
            pCmdBuffer->SetAllScissors(m_info.scissorRectParams, newTokens.scissorRect);
        }
    }
    else if (ContainsStaticState(DynamicStatesInternal::ScissorCount))
    {
        utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
        do
        {
            uint32* pScissorCount = &(pCmdBuffer->PerGpuState(deviceGroup.Index())->scissor.count);

            if (*pScissorCount != m_info.scissorRectParams.count)
            {
                *pScissorCount = m_info.scissorRectParams.count;
                pRenderState->dirtyGraphics.scissor = 1;
            }
        }
        while (deviceGroup.IterateNext());
    }

    if (m_flags.bindDepthStencilObject == false)
    {
        Pal::DepthStencilStateCreateInfo* pDepthStencilCreateInfo = &(pRenderState->depthStencilCreateInfo);

        if (ContainsStaticState(DynamicStatesInternal::DepthTestEnable) &&
            (pDepthStencilCreateInfo->depthEnable != m_info.depthStencilCreateInfo.depthEnable))
        {
            pDepthStencilCreateInfo->depthEnable = m_info.depthStencilCreateInfo.depthEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthWriteEnable) &&
            (pDepthStencilCreateInfo->depthWriteEnable != m_info.depthStencilCreateInfo.depthWriteEnable))
        {
            pDepthStencilCreateInfo->depthWriteEnable = m_info.depthStencilCreateInfo.depthWriteEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthCompareOp) &&
            (pDepthStencilCreateInfo->depthFunc != m_info.depthStencilCreateInfo.depthFunc))
        {
            pDepthStencilCreateInfo->depthFunc = m_info.depthStencilCreateInfo.depthFunc;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthBoundsTestEnable) &&
            (pDepthStencilCreateInfo->depthBoundsEnable != m_info.depthStencilCreateInfo.depthBoundsEnable))
        {
            pDepthStencilCreateInfo->depthBoundsEnable = m_info.depthStencilCreateInfo.depthBoundsEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilTestEnable) &&
            (pDepthStencilCreateInfo->stencilEnable != m_info.depthStencilCreateInfo.stencilEnable))
        {
            pDepthStencilCreateInfo->stencilEnable = m_info.depthStencilCreateInfo.stencilEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilOp) &&
            ((pDepthStencilCreateInfo->front.stencilFailOp != m_info.depthStencilCreateInfo.front.stencilFailOp) ||
             (pDepthStencilCreateInfo->front.stencilPassOp != m_info.depthStencilCreateInfo.front.stencilPassOp) ||
             (pDepthStencilCreateInfo->front.stencilDepthFailOp !=
              m_info.depthStencilCreateInfo.front.stencilDepthFailOp) ||
             (pDepthStencilCreateInfo->front.stencilFunc != m_info.depthStencilCreateInfo.front.stencilFunc) ||
             (pDepthStencilCreateInfo->back.stencilFailOp != m_info.depthStencilCreateInfo.back.stencilFailOp) ||
             (pDepthStencilCreateInfo->back.stencilPassOp != m_info.depthStencilCreateInfo.back.stencilPassOp) ||
             (pDepthStencilCreateInfo->back.stencilDepthFailOp !=
              m_info.depthStencilCreateInfo.back.stencilDepthFailOp) ||
             (pDepthStencilCreateInfo->back.stencilFunc != m_info.depthStencilCreateInfo.back.stencilFunc)))
        {
            pDepthStencilCreateInfo->front = m_info.depthStencilCreateInfo.front;
            pDepthStencilCreateInfo->back  = m_info.depthStencilCreateInfo.back;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
    }
    else
    {
        // Update static state to cmdBuffer when it's static DS. It's necessary because
        // it will be used to tell if the value is changed next time.
        pRenderState->depthStencilCreateInfo = m_info.depthStencilCreateInfo;
    }

    if (m_flags.bindColorBlendObject == false)
    {
        if (ContainsStaticState(DynamicStatesInternal::ColorBlendEnable))
        {
            for (uint32_t i = 0; i < Pal::MaxColorTargets; i++)
            {
                if (pRenderState->colorBlendCreateInfo.targets[i].blendEnable !=
                    m_info.blendCreateInfo.targets[i].blendEnable)
                {
                    pRenderState->colorBlendCreateInfo.targets[i].blendEnable =
                        m_info.blendCreateInfo.targets[i].blendEnable;
                    pRenderState->dirtyGraphics.colorBlend = 1;
                }
            }
        }

        if (ContainsStaticState(DynamicStatesInternal::ColorBlendEquation))
        {
            for (uint32_t i = 0; i < Pal::MaxColorTargets; i++)
            {
                const auto& srcTarget  = m_info.blendCreateInfo.targets[i];
                auto        pDstTarget = &pRenderState->colorBlendCreateInfo.targets[i];

                if ((pDstTarget->srcBlendColor  != srcTarget.srcBlendColor) ||
                    (pDstTarget->dstBlendColor  != srcTarget.dstBlendColor) ||
                    (pDstTarget->blendFuncColor != srcTarget.blendFuncColor) ||
                    (pDstTarget->srcBlendAlpha  != srcTarget.srcBlendAlpha) ||
                    (pDstTarget->dstBlendAlpha  != srcTarget.dstBlendAlpha) ||
                    (pDstTarget->blendFuncAlpha != srcTarget.blendFuncAlpha))

                {
                    pDstTarget->srcBlendColor  = srcTarget.srcBlendColor;
                    pDstTarget->dstBlendColor  = srcTarget.dstBlendColor;
                    pDstTarget->blendFuncColor = srcTarget.blendFuncColor;
                    pDstTarget->srcBlendAlpha  = srcTarget.srcBlendAlpha;
                    pDstTarget->dstBlendAlpha  = srcTarget.dstBlendAlpha;
                    pDstTarget->blendFuncAlpha = srcTarget.blendFuncAlpha;
                    pRenderState->dirtyGraphics.colorBlend = 1;
                }
            }
        }
    }
    else
    {
        pRenderState->colorBlendCreateInfo = m_info.blendCreateInfo;
    }

    if (m_flags.bindMsaaObject == false)
    {
        if (ContainsStaticState(DynamicStatesInternal::SampleMask))
        {
            if (pRenderState->msaaCreateInfo.sampleMask != m_info.msaaCreateInfo.sampleMask)
            {
                pRenderState->msaaCreateInfo.sampleMask = m_info.msaaCreateInfo.sampleMask;
                pRenderState->dirtyGraphics.msaa = 1;
            }
        }

        if (ContainsStaticState(DynamicStatesInternal::ConservativeRasterizationMode))
        {
            if ((pRenderState->msaaCreateInfo.conservativeRasterizationMode !=
                m_info.msaaCreateInfo.conservativeRasterizationMode) ||
                (pRenderState->msaaCreateInfo.flags.enableConservativeRasterization !=
                    m_info.msaaCreateInfo.flags.enableConservativeRasterization))
            {
                pRenderState->msaaCreateInfo.conservativeRasterizationMode =
                    m_info.msaaCreateInfo.conservativeRasterizationMode;
                pRenderState->msaaCreateInfo.flags.enableConservativeRasterization =
                    m_info.msaaCreateInfo.flags.enableConservativeRasterization;
                pRenderState->dirtyGraphics.msaa = 1;
            }
        }

        if (ContainsStaticState(DynamicStatesInternal::LineStippleEnable))
        {
            if (pRenderState->msaaCreateInfo.flags.enableLineStipple != m_info.msaaCreateInfo.flags.enableLineStipple)
            {
                pRenderState->msaaCreateInfo.flags.enableLineStipple = m_info.msaaCreateInfo.flags.enableLineStipple;
                pRenderState->dirtyGraphics.msaa = 1;
            }
        }

        if (ContainsStaticState(DynamicStatesInternal::RasterizationSamples))
        {
            if (pRenderState->dirtyGraphics.msaa == 0)
            {
                if (memcmp(&pRenderState->msaaCreateInfo, &m_info.msaaCreateInfo, sizeof(m_info.msaaCreateInfo)) != 0)
                {
                    pRenderState->dirtyGraphics.msaa = 1;
                }
            }

            if (pRenderState->dirtyGraphics.msaa)
            {
                pRenderState->msaaCreateInfo.coverageSamples         = m_info.msaaCreateInfo.coverageSamples;
                pRenderState->msaaCreateInfo.exposedSamples          = m_info.msaaCreateInfo.exposedSamples;
                pRenderState->msaaCreateInfo.pixelShaderSamples      = m_info.msaaCreateInfo.pixelShaderSamples;
                pRenderState->msaaCreateInfo.depthStencilSamples     = m_info.msaaCreateInfo.depthStencilSamples;
                pRenderState->msaaCreateInfo.shaderExportMaskSamples = m_info.msaaCreateInfo.shaderExportMaskSamples;
                pRenderState->msaaCreateInfo.sampleClusters          = m_info.msaaCreateInfo.sampleClusters;
                pRenderState->msaaCreateInfo.alphaToCoverageSamples  = m_info.msaaCreateInfo.alphaToCoverageSamples;
                pRenderState->msaaCreateInfo.occlusionQuerySamples   = m_info.msaaCreateInfo.occlusionQuerySamples;

                if (m_flags.customSampleLocations)
                {
                    pRenderState->msaaCreateInfo.flags.enable1xMsaaSampleLocations =
                        m_info.msaaCreateInfo.flags.enable1xMsaaSampleLocations;
                }
                pRenderState->msaaCreateInfo.flags.forceSampleRateShading =
                    m_info.msaaCreateInfo.flags.forceSampleRateShading;
            }
        }
        else
        {
            uint32_t pixelShaderSamples = 1;
            if (m_info.minSampleShading > 0.0f)
            {
                pixelShaderSamples = Pow2Pad(static_cast<uint32_t>(ceil(
                    pRenderState->msaaCreateInfo.coverageSamples * m_info.minSampleShading)));
            }

            if (pRenderState->msaaCreateInfo.pixelShaderSamples != pixelShaderSamples)
            {
                pRenderState->msaaCreateInfo.pixelShaderSamples = pixelShaderSamples;
                pRenderState->dirtyGraphics.msaa = 1;
            }
        }
    }
    else
    {
        pRenderState->msaaCreateInfo = m_info.msaaCreateInfo;
    }

    pRenderState->minSampleShading = m_info.minSampleShading;

    if (m_flags.bindTriangleRasterState == false)
    {
        // Update the static states to renderState
        if (ContainsStaticState(DynamicStatesInternal::FrontFace))
        {
            pRenderState->triangleRasterState.frontFace = m_info.triangleRasterState.frontFace;
        }

        if (ContainsStaticState(DynamicStatesInternal::CullMode))
        {
            pRenderState->triangleRasterState.cullMode = m_info.triangleRasterState.cullMode;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBiasEnable))
        {
            pRenderState->triangleRasterState.flags.frontDepthBiasEnable =
                m_info.triangleRasterState.flags.frontDepthBiasEnable;
            pRenderState->triangleRasterState.flags.backDepthBiasEnable =
                m_info.triangleRasterState.flags.backDepthBiasEnable;
        }

        if (ContainsStaticState(DynamicStatesInternal::PolygonMode))
        {
            pRenderState->triangleRasterState.frontFillMode = m_info.triangleRasterState.frontFillMode;
            pRenderState->triangleRasterState.backFillMode = m_info.triangleRasterState.backFillMode;
        }

        if (ContainsStaticState(DynamicStatesInternal::ProvokingVertexMode))
        {
            pRenderState->triangleRasterState.provokingVertex = m_info.triangleRasterState.provokingVertex;
        }

        pRenderState->dirtyGraphics.rasterState = 1;
    }
    else
    {
        pRenderState->triangleRasterState = m_info.triangleRasterState;
    }

    Pal::StencilRefMaskParams prevStencilRefMasks = pRenderState->stencilRefMasks;

    if (m_flags.bindStencilRefMasks == false)
    {
        // Until we expose Stencil Op Value, we always inherit the PSO value, which is currently Default == 1
        pRenderState->stencilRefMasks.frontOpValue   = m_info.stencilRefMasks.frontOpValue;
        pRenderState->stencilRefMasks.backOpValue    = m_info.stencilRefMasks.backOpValue;

        // We don't have to use tokens for these since the combiner does a redundancy check on the full value
        if (ContainsStaticState(DynamicStatesInternal::StencilCompareMask))
        {
            pRenderState->stencilRefMasks.frontReadMask  = m_info.stencilRefMasks.frontReadMask;
            pRenderState->stencilRefMasks.backReadMask   = m_info.stencilRefMasks.backReadMask;
        }

        if (ContainsStaticState(DynamicStatesInternal::StencilWriteMask))
        {
            pRenderState->stencilRefMasks.frontWriteMask = m_info.stencilRefMasks.frontWriteMask;
            pRenderState->stencilRefMasks.backWriteMask  = m_info.stencilRefMasks.backWriteMask;
        }

        if (ContainsStaticState(DynamicStatesInternal::StencilReference))
        {
            pRenderState->stencilRefMasks.frontRef       = m_info.stencilRefMasks.frontRef;
            pRenderState->stencilRefMasks.backRef        = m_info.stencilRefMasks.backRef;
        }
    }
    else
    {
        pRenderState->stencilRefMasks = m_info.stencilRefMasks;
    }

    // Check whether the dirty bit should be set
    if (memcmp(&pRenderState->stencilRefMasks, &prevStencilRefMasks, sizeof(Pal::StencilRefMaskParams)) != 0)
    {
        pRenderState->dirtyGraphics.stencilRef = 1;
    }

    if (m_flags.bindInputAssemblyState == false)
    {
        // Update the static states to renderState
        if (ContainsStaticState(DynamicStatesInternal::PrimitiveRestartEnable))
        {
            pRenderState->inputAssemblyState.primitiveRestartEnable = m_info.inputAssemblyState.primitiveRestartEnable;
        }

        pRenderState->inputAssemblyState.primitiveRestartIndex = m_info.inputAssemblyState.primitiveRestartIndex;
        pRenderState->inputAssemblyState.patchControlPoints    = m_info.inputAssemblyState.patchControlPoints;

        if (ContainsStaticState(DynamicStatesInternal::PrimitiveTopology))
        {
            pRenderState->inputAssemblyState.topology = m_info.inputAssemblyState.topology;
        }

        pRenderState->dirtyGraphics.inputAssembly = 1;
    }
    else
    {
        pRenderState->inputAssemblyState = m_info.inputAssemblyState;
    }

    const uint64_t oldHash = pRenderState->boundGraphicsPipelineHash;
    const uint64_t newHash = PalPipelineHash();
    const bool dynamicStateDirty =
        pGfxDynamicBindInfo->enable.u32All != dynamicGraphicsState.enable.u32All;

    // Update pipeline dynamic state
    pGfxDynamicBindInfo->enable.u32All = dynamicGraphicsState.enable.u32All;

    if (ContainsStaticState(DynamicStatesInternal::ColorWriteMask) ^
        ContainsStaticState(DynamicStatesInternal::ColorWriteEnable))
    {
        if (ContainsStaticState(DynamicStatesInternal::ColorWriteMask))
        {
            pRenderState->colorWriteMask = m_info.colorWriteMask;
        }

        if (ContainsStaticState(DynamicStatesInternal::ColorWriteEnable))
        {
            pRenderState->colorWriteEnable = m_info.colorWriteEnable;
        }

        pRenderState->dirtyGraphics.colorWriteMask = 1;
    }

    // Overwrite state with dynamic state in current render state
    if (dynamicGraphicsState.enable.logicOp)
    {
        if (ContainsStaticState(DynamicStatesInternal::LogicOp))
        {
            pRenderState->logicOp = m_info.logicOp;
            pGfxDynamicBindInfo->logicOp =
                pRenderState->logicOpEnable ? VkToPalLogicOp(pRenderState->logicOp) : Pal::LogicOp::Copy;
        }

        if (ContainsStaticState(DynamicStatesInternal::LogicOpEnable))
        {
            pRenderState->logicOpEnable = m_info.logicOpEnable;
            pGfxDynamicBindInfo->logicOp =
                pRenderState->logicOpEnable ? VkToPalLogicOp(pRenderState->logicOp) : Pal::LogicOp::Copy;
        }
    }
    else
    {
        pRenderState->logicOp       = m_info.logicOp;
        pRenderState->logicOpEnable = m_info.logicOpEnable;
    }

    utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        pCmdBuffer->GetDebugPrintf()->BindPipeline(m_pDevice,
                                                   this,
                                                   deviceIdx,
                                                   pPalCmdBuf,
                                                   static_cast<uint32_t>(Pal::PipelineBindPoint::Graphics),
                                                   m_userDataLayout.common.debugPrintfRegBase);
        if ((oldHash != newHash)
            || dynamicStateDirty
            )
        {
            pRenderState->dirtyGraphics.pipeline = 1;
        }

        // Bind state objects that are always static; these are redundancy checked by the pointer in the command buffer.
        if (m_flags.bindDepthStencilObject)
        {
            pCmdBuffer->PalCmdBindDepthStencilState(pPalCmdBuf, deviceIdx, m_pPalDepthStencil[deviceIdx]);

            pRenderState->dirtyGraphics.depthStencil = 0;
        }

        if (m_flags.bindColorBlendObject)
        {
            pCmdBuffer->PalCmdBindColorBlendState(pPalCmdBuf, deviceIdx, m_pPalColorBlend[deviceIdx]);
            pRenderState->dirtyGraphics.colorBlend = 0;
        }

        if (m_flags.bindMsaaObject)
        {
            pCmdBuffer->PalCmdBindMsaaState(pPalCmdBuf, deviceIdx, m_pPalMsaa[deviceIdx]);
            pRenderState->dirtyGraphics.msaa = 0;
        }

        // Write parameters that are marked static pipeline state.  Redundancy check these based on static tokens:
        // skip the write if the previously written static token matches.

        if (CmdBuffer::IsStaticStateDifferent(oldTokens.inputAssemblyState, newTokens.inputAssemblyState) &&
                m_flags.bindInputAssemblyState)
        {
            pPalCmdBuf->CmdSetInputAssemblyState(m_info.inputAssemblyState);

            pRenderState->staticTokens.inputAssemblyState = newTokens.inputAssemblyState;
            pRenderState->dirtyGraphics.inputAssembly             = 0;
        }

        if (CmdBuffer::IsStaticStateDifferent(oldTokens.triangleRasterState, newTokens.triangleRasterState) &&
                m_flags.bindTriangleRasterState)
        {
            pPalCmdBuf->CmdSetTriangleRasterState(m_info.triangleRasterState);

            pRenderState->staticTokens.triangleRasterState = newTokens.triangleRasterState;
            pRenderState->dirtyGraphics.rasterState                = 0;
        }

        if (ContainsStaticState(DynamicStatesInternal::LineWidth) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.pointLineRasterState, newTokens.pointLineRasterState))
        {
            pPalCmdBuf->CmdSetPointLineRasterState(m_info.pointLineRasterParams);
            pRenderState->staticTokens.pointLineRasterState = newTokens.pointLineRasterState;
        }

        if (ContainsStaticState(DynamicStatesInternal::LineStipple) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.lineStippleState, newTokens.lineStippleState))
        {
            pPalCmdBuf->CmdSetLineStippleState(m_info.lineStippleParams);
            pRenderState->staticTokens.lineStippleState = newTokens.lineStippleState;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBias) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBiasState, newTokens.depthBias))
        {
            pPalCmdBuf->CmdSetDepthBiasState(m_info.depthBiasParams);
            pRenderState->staticTokens.depthBiasState = newTokens.depthBias;
        }

        if (ContainsStaticState(DynamicStatesInternal::BlendConstants) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.blendConst, newTokens.blendConst))
        {
            pPalCmdBuf->CmdSetBlendConst(m_info.blendConstParams);
            pRenderState->staticTokens.blendConst = newTokens.blendConst;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBounds) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBounds, newTokens.depthBounds))
        {
            pPalCmdBuf->CmdSetDepthBounds(m_info.depthBoundParams);
            pRenderState->staticTokens.depthBounds = newTokens.depthBounds;
        }

        if (ContainsStaticState(DynamicStatesInternal::SampleLocations) &&
            ContainsStaticState(DynamicStatesInternal::SampleLocationsEnable) &&
            ContainsStaticState(DynamicStatesInternal::RasterizationSamples))
        {
            if ((pRenderState->sampleLocationsEnable != m_flags.customSampleLocations) ||
                (memcmp(&pRenderState->samplePattern, &m_info.samplePattern, sizeof(SamplePattern)) != 0))
            {
                pCmdBuffer->PalCmdSetMsaaQuadSamplePattern(
                    m_info.samplePattern.sampleCount, m_info.samplePattern.locations);
                pRenderState->samplePattern = m_info.samplePattern;
                pRenderState->sampleLocationsEnable = m_flags.customSampleLocations;
                pRenderState->dirtyGraphics.samplePattern = 0;
            }
        }
        else
        {
            if (ContainsStaticState(DynamicStatesInternal::SampleLocations))
            {
                if (memcmp(&pRenderState->samplePattern.locations,
                           &m_info.samplePattern.locations,
                           sizeof(Pal::MsaaQuadSamplePattern)) != 0)
                {
                    pRenderState->samplePattern.locations = m_info.samplePattern.locations;
                    pRenderState->dirtyGraphics.samplePattern = 1;
                }
            }
            if (ContainsStaticState(DynamicStatesInternal::SampleLocationsEnable))
            {
                if (pRenderState->sampleLocationsEnable != m_flags.customSampleLocations)
                {
                    pRenderState->sampleLocationsEnable = m_flags.customSampleLocations;
                    pRenderState->dirtyGraphics.samplePattern = 1;
                }
            }
            if (ContainsStaticState(DynamicStatesInternal::RasterizationSamples))
            {
                if (pRenderState->samplePattern.sampleCount != m_info.samplePattern.sampleCount)
                {
                    pRenderState->samplePattern.sampleCount = m_info.samplePattern.sampleCount;
                    pRenderState->dirtyGraphics.samplePattern = 1;
                }
            }
        }

        // Only set the Fragment Shading Rate if the dynamic state is not set.
        if (m_flags.fragmentShadingRateEnable)
        {
            if (ContainsStaticState(DynamicStatesInternal::FragmentShadingRateStateKhr))
            {
                if (CmdBuffer::IsStaticStateDifferent(oldTokens.fragmentShadingRate, newTokens.fragmentShadingRate))
                {
                    pPalCmdBuf->CmdSetPerDrawVrsRate(m_info.vrsRateParams);

                    pRenderState->staticTokens.fragmentShadingRate = newTokens.fragmentShadingRate;
                    pRenderState->dirtyGraphics.vrs = 0;
                }
            }
            else
            {
                if (m_info.minSampleShading > 0.0)
                {
                    if ((pRenderState->vrsRate.shadingRate == Pal::VrsShadingRate::_1x1) &&
                        (m_flags.shadingRateUsedInShader == false))
                    {
                        pRenderState->dirtyGraphics.vrs = 1;
                    }
                }
            }
        }

        if (m_internalBufferInfo.internalBufferCount > 0)
        {
            for (uint32_t i = 0; i < m_internalBufferInfo.internalBufferCount; i++)
            {
                const InternalBufferEntry& buffEntry = m_internalBufferInfo.internalBufferEntries[i];
                uint32_t internalBufferLow = buffEntry.bufferAddress[deviceIdx] & UINT32_MAX;
                pPalCmdBuf->CmdSetUserData(Pal::PipelineBindPoint::Graphics,
                    buffEntry.userDataOffset,
                    1,
                    &internalBufferLow);
            }
        }
    }
    while (deviceGroup.IterateNext());

    pRenderState->boundGraphicsPipelineHash = newHash;

    // Binding GraphicsPipeline affects ViewMask,
    // because when VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT is specified
    // ViewMask for each VkPhysicalDevice is defined by DeviceIndex
    // not by current subpass during a render pass instance.
    const uint32_t oldViewIndexFromDeviceIndex = pRenderState->viewIndexFromDeviceIndex;
    const uint32_t newViewIndexFromDeviceIndex = StageMaskForViewIndexUseDeviceIndex();

    if (oldViewIndexFromDeviceIndex != newViewIndexFromDeviceIndex)
    {
        // Update value of ViewIndexFromDeviceIndex for currently bound pipeline.
        pRenderState->viewIndexFromDeviceIndex = newViewIndexFromDeviceIndex;

        // Sync ViewMask state in CommandBuffer.
        pCmdBuffer->SetViewInstanceMask(pCmdBuffer->GetDeviceMask());
    }
}

} // namespace vk
