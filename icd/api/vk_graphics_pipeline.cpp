/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_shader.h"
#include "include/vk_cmdbuffer.h"

#include "palAutoBuffer.h"
#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palPipeline.h"
#include "palInlineFuncs.h"
#include "palMetroHash.h"
#include "palVectorImpl.h"

#include <float.h>
#include <math.h>

using namespace Util;

namespace vk
{

// =====================================================================================================================
// Create graphics pipeline binaries
VkResult GraphicsPipeline::CreatePipelineBinaries(
    Device*                                        pDevice,
    const VkGraphicsPipelineCreateInfo*            pCreateInfo,
    PipelineCreateFlags                            flags,
    const GraphicsPipelineShaderStageInfo*         pShaderInfo,
    const PipelineLayout*                          pPipelineLayout,
    const Util::MetroHash::Hash*                   pElfHash,
    PipelineOptimizerKey*                          pPipelineOptimizerKey,
    GraphicsPipelineBinaryCreateInfo*              pBinaryCreateInfo,
    PipelineCache*                                 pPipelineCache,
    const VkPipelineCreationFeedbackCreateInfoEXT* pCreationFeedbackInfo,
    Util::MetroHash::Hash*                         pCacheIds,
    size_t*                                        pPipelineBinarySizes,
    const void**                                   ppPipelineBinaries,
    PipelineMetadata*                              pBinaryMetadata)
{
    VkResult          result           = VK_SUCCESS;
    const uint32_t    numPalDevices    = pDevice->NumPalDevices();
    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    // Load or create the pipeline binary
    PipelineBinaryCache* pPipelineBinaryCache = (pPipelineCache != nullptr) ? pPipelineCache->GetPipelineCache()
                                                                            : nullptr;

    for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < numPalDevices); ++deviceIdx)
    {
        bool isUserCacheHit     = false;
        bool isInternalCacheHit = false;

        ElfHashToCacheId(
            pDevice,
            deviceIdx,
            *pElfHash,
            pDevice->VkPhysicalDevice(deviceIdx)->GetSettingsLoader()->GetSettingsHash(),
            *pPipelineOptimizerKey,
            &pCacheIds[deviceIdx]
        );

        bool shouldCompile = true;

        if (shouldCompile)
        {
            bool skipCaching = pDevice->GetRuntimeSettings().enablePipelineDump;

            if (skipCaching == false)
            {
                // Search the pipeline binary cache
                Util::Result cacheResult = pDevice->GetCompiler(deviceIdx)->GetCachedPipelineBinary(
                    &pCacheIds[deviceIdx],
                    pPipelineBinaryCache,
                    &pPipelineBinarySizes[deviceIdx],
                    &ppPipelineBinaries[deviceIdx],
                    &isUserCacheHit,
                    &isInternalCacheHit,
                    &pBinaryCreateInfo->freeCompilerBinary,
                    &pBinaryCreateInfo->pipelineFeedback);

                // Compile if not found in cache
                shouldCompile = (cacheResult != Util::Result::Success);
            }
        }

        // Compile if unable to retrieve from cache
        if (shouldCompile)
        {
            if ((deviceIdx == DefaultDeviceIndex) || (pCreateInfo == nullptr))
            {
                if (pCreateInfo != nullptr)
                {
                    result = pDefaultCompiler->ConvertGraphicsPipelineInfo(
                        pDevice,
                        pCreateInfo,
                        flags,
                        pShaderInfo,
                        pPipelineLayout,
                        pPipelineOptimizerKey,
                        pBinaryMetadata,
                        pBinaryCreateInfo);
                }

                if (result == VK_SUCCESS)
                {
                    result = pDefaultCompiler->CreateGraphicsPipelineBinary(
                        pDevice,
                        deviceIdx,
                        pPipelineCache,
                        pBinaryCreateInfo,
                        flags,
                        &pPipelineBinarySizes[deviceIdx],
                        &ppPipelineBinaries[deviceIdx],
                        &pCacheIds[deviceIdx]);

                    if (result == VK_SUCCESS && (pPipelineBinarySizes[deviceIdx] > 0))
                    {
                        result = pDefaultCompiler->WriteBinaryMetadata(
                            pDevice,
                            pBinaryCreateInfo->compilerType,
                            &pBinaryCreateInfo->freeCompilerBinary,
                            &ppPipelineBinaries[deviceIdx],
                            &pPipelineBinarySizes[deviceIdx],
                            pBinaryCreateInfo->pBinaryMetadata);
                    }
                }
            }
            else
            {
                GraphicsPipelineBinaryCreateInfo binaryCreateInfoMGPU = {};
                PipelineMetadata                 binaryMetadataMGPU   = {};
                result = pDefaultCompiler->ConvertGraphicsPipelineInfo(
                    pDevice,
                    pCreateInfo,
                    flags,
                    pShaderInfo,
                    pPipelineLayout,
                    pPipelineOptimizerKey,
                    &binaryMetadataMGPU,
                    &binaryCreateInfoMGPU);

                if (result == VK_SUCCESS)
                {
                    result = pDevice->GetCompiler(deviceIdx)->CreateGraphicsPipelineBinary(
                        pDevice,
                        deviceIdx,
                        pPipelineCache,
                        &binaryCreateInfoMGPU,
                        flags,
                        &pPipelineBinarySizes[deviceIdx],
                        &ppPipelineBinaries[deviceIdx],
                        &pCacheIds[deviceIdx]);
                }

                if (result == VK_SUCCESS)
                {
                    result = pDefaultCompiler->SetPipelineCreationFeedbackInfo(
                        pCreationFeedbackInfo,
                        pCreateInfo->stageCount,
                        pCreateInfo->pStages,
                        &binaryCreateInfoMGPU.pipelineFeedback,
                        binaryCreateInfoMGPU.stageFeedback);
                }

                if (binaryMetadataMGPU.internalBufferInfo.pData != nullptr)
                {
                    pDevice->VkInstance()->FreeMem(binaryMetadataMGPU.internalBufferInfo.pData);
                    binaryMetadataMGPU.internalBufferInfo.pData = nullptr;
                }

                pDefaultCompiler->FreeGraphicsPipelineCreateInfo(&binaryCreateInfoMGPU, false);
            }
        }
        else if (deviceIdx == DefaultDeviceIndex)
        {
            pDefaultCompiler->ReadBinaryMetadata(
                pDevice,
                ppPipelineBinaries[DefaultDeviceIndex],
                pPipelineBinarySizes[DefaultDeviceIndex],
                pBinaryMetadata);
        }

        // Add to any cache layer where missing
        if (result == VK_SUCCESS)
        {
            // Only store the optimized variant of the pipeline if deferCompileOptimizedPipeline is enabled
            if ((pPipelineBinarySizes[deviceIdx] != 0) &&
                ((pDevice->GetRuntimeSettings().deferCompileOptimizedPipeline == false) ||
                ((pBinaryMetadata->enableEarlyCompile == false) &&
                 (pBinaryMetadata->enableUberFetchShader == false))))
            {
                pDevice->GetCompiler(deviceIdx)->CachePipelineBinary(
                    &pCacheIds[deviceIdx],
                    pPipelineBinaryCache,
                    pPipelineBinarySizes[deviceIdx],
                    ppPipelineBinaries[deviceIdx],
                    isUserCacheHit,
                    isInternalCacheHit);
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
    const size_t*                     pPipelineBinarySizes,
    const void**                      pPipelineBinaries,
    const Util::MetroHash::Hash*      pCacheIds,
    void*                             pSystemMem,
    Pal::IPipeline**                  pPalPipeline)
{
    size_t palSize = 0;

    pObjectCreateInfo->pipeline.pipelineBinarySize = pPipelineBinarySizes[DefaultDeviceIndex];
    pObjectCreateInfo->pipeline.pPipelineBinary    = pPipelineBinaries[DefaultDeviceIndex];

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
            if (pPipelineBinaries[deviceIdx] != nullptr)
            {
                pObjectCreateInfo->pipeline.pipelineBinarySize = pPipelineBinarySizes[deviceIdx];
                pObjectCreateInfo->pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
            }

            palResult = pPalDevice->CreateGraphicsPipeline(
                pObjectCreateInfo->pipeline,
                Util::VoidPtrInc(pSystemMem, palOffset),
                &pPalPipeline[deviceIdx]);

#if ICD_GPUOPEN_DEVMODE_BUILD
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
#endif

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
    PipelineCreateFlags                 flags,
    const VkAllocationCallbacks*        pAllocator,
    const PipelineLayout*               pPipelineLayout,
    const VbBindingInfo*                pVbInfo,
    const PipelineInternalBufferInfo*   pInternalBuffer,
    const size_t*                       pPipelineBinarySizes,
    const void**                        pPipelineBinaries,
    PipelineCache*                      pPipelineCache,
    const Util::MetroHash::Hash*        pCacheIds,
    uint64_t                            apiPsoHash,
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

    palSize =
        pDevice->PalDevice(DefaultDeviceIndex)->GetGraphicsPipelineSize(pObjectCreateInfo->pipeline, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    pSystemMem = pDevice->AllocApiObject(
        pAllocator,
        sizeof(GraphicsPipeline) + (palSize * numPalDevices) + pInternalBuffer->dataSize);

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
            pPipelineBinarySizes,
            pPipelineBinaries,
            pCacheIds,
            Util::VoidPtrInc(pSystemMem, sizeof(GraphicsPipeline)),
            pPalPipeline);
    }

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

                    if ((info.ps.flags.perSampleShading == 1) ||
                        ((info.ps.flags.usesSampleMask  == 1) &&
                         (palProperties.gfxipProperties.flags.supportVrsWithDsExports == 0)))
                    {
                        // Override the shader rate to 1x1 if SampleId used in shader or
                        // supportVrsWithDsExports is not supported and SampleMask used in shader.
                        Force1x1ShaderRate(&pObjectCreateInfo->immedInfo.vrsRateParams);
                        pObjectCreateInfo->flags.force1x1ShaderRate = true;
                        if (pObjectCreateInfo->flags.bindMsaaObject == false)
                        {
                            pObjectCreateInfo->flags.sampleShadingEnable = true;
                            pObjectCreateInfo->immedInfo.minSampleShading = 1.0f;
                        }
                        pObjectCreateInfo->immedInfo.msaaCreateInfo.pixelShaderSamples =
                            pObjectCreateInfo->immedInfo.msaaCreateInfo.coverageSamples;
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
    }

    // On success, wrap it up in a Vulkan object.
    if (result == VK_SUCCESS)
    {
        PipelineInternalBufferInfo internalBuffer = *pInternalBuffer;
        if (pInternalBuffer->dataSize > 0)
        {
            internalBuffer.pData = Util::VoidPtrInc(pSystemMem, sizeof(GraphicsPipeline) + (palSize * numPalDevices));
            memcpy(internalBuffer.pData, pInternalBuffer->pData, pInternalBuffer->dataSize);
        }

        VK_PLACEMENT_NEW(pSystemMem) GraphicsPipeline(
            pDevice,
            pPalPipeline,
            pPipelineLayout,
            pObjectCreateInfo->immedInfo,
            pObjectCreateInfo->staticStateMask,
            pObjectCreateInfo->flags,
#if VKI_RAY_TRACING
            pObjectCreateInfo->dispatchRaysUserDataOffset,
#endif
            *pVbInfo,
            &internalBuffer,
            pPalMsaa,
            pPalColorBlend,
            pPalDepthStencil,
            pObjectCreateInfo->sampleCoverage,
            pCacheIds[DefaultDeviceIndex],
            apiPsoHash,
            &palPipelineHasher);

        *pPipeline = GraphicsPipeline::HandleFromVoidPointer(pSystemMem);
        if (pDevice->GetRuntimeSettings().enableDebugPrintf)
        {
            GraphicsPipeline* pGraphicsPipeline = static_cast<GraphicsPipeline*>(pSystemMem);
            pGraphicsPipeline->ClearFormatString();
            DebugPrintf::DecodeFormatStringsFromElf(pDevice,
                                                    pPipelineBinarySizes[DefaultDeviceIndex],
                                                    static_cast<const char*>(pPipelineBinaries[DefaultDeviceIndex]),
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
// Create a graphics pipeline object.
VkResult GraphicsPipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    PipelineCreateFlags                     flags,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo = nullptr;

    pDefaultCompiler->GetPipelineCreationFeedback(static_cast<const VkStructHeader*>(pCreateInfo->pNext),
                                                  &pPipelineCreationFeedbackCreateInfo);

    // 1. Build shader stage infos
    GraphicsPipelineBinaryCreateInfo binaryCreateInfo   = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo    = {};
    ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount] = {};

    VkResult result = BuildShaderStageInfo(pDevice,
                                           pCreateInfo->stageCount,
                                           pCreateInfo->pStages,
                                           flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                                           [](const uint32_t inputIdx, const uint32_t stageIdx)
                                           {
                                               return stageIdx;
                                           },
                                           shaderStageInfo.stages,
                                           tempModules,
                                           pPipelineCache,
                                           binaryCreateInfo.stageFeedback);

    // 2. Build ShaderOptimizer pipeline key
    PipelineOptimizerKey pipelineOptimizerKey = {};
    ShaderOptimizerKey   shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount] = {};
    if (result == VK_SUCCESS)
    {
        GeneratePipelineOptimizerKey(
            pDevice, pCreateInfo, flags, &shaderStageInfo, shaderOptimizerKeys, &pipelineOptimizerKey);
    }

    // 3. Build API and ELF hashes
    uint64_t              apiPsoHash = {};
    Util::MetroHash::Hash elfHash    = {};
    BuildApiHash(pCreateInfo, flags, &apiPsoHash, &elfHash);

    binaryCreateInfo.apiPsoHash = apiPsoHash;

    // 4. Get pipeline layout
    VK_ASSERT(pCreateInfo->layout != VK_NULL_HANDLE);
    PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    // 5. Create pipeine binaries (or load from cache)
    size_t                     pipelineBinarySizes[MaxPalDevices] = {};
    const void*                pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash      cacheId[MaxPalDevices]             = {};
    PipelineMetadata           binaryMetadata                     = {};

    if (result == VK_SUCCESS)
    {
        result = CreatePipelineBinaries(
            pDevice,
            pCreateInfo,
            flags,
            &shaderStageInfo,
            pPipelineLayout,
            &elfHash,
            &pipelineOptimizerKey,
            &binaryCreateInfo,
            pPipelineCache,
            pPipelineCreationFeedbackCreateInfo,
            cacheId,
            pipelineBinarySizes,
            pPipelineBinaries,
            &binaryMetadata);
    }

    GraphicsPipelineObjectCreateInfo objectCreateInfo = {};
    if (result == VK_SUCCESS)
    {
        // 6. Build pipeline object create info
        BuildPipelineObjectCreateInfo(
            pDevice,
            pCreateInfo,
            flags,
            &shaderStageInfo,
            pPipelineLayout,
            &pipelineOptimizerKey,
            &binaryMetadata,
            &objectCreateInfo);

        if (pDevice->GetRuntimeSettings().useShaderLibraryForPipelineLibraryFastLink)
        {
            result = PrepareShaderLibrary(pDevice, pAllocator, &binaryCreateInfo, &objectCreateInfo);
        }

        if (result == VK_SUCCESS)
        {
            objectCreateInfo.immedInfo.checkDeferCompilePipeline =
                pDevice->GetRuntimeSettings().deferCompileOptimizedPipeline &&
                (binaryMetadata.enableEarlyCompile || binaryMetadata.enableUberFetchShader);

#if VKI_RAY_TRACING
            objectCreateInfo.flags.hasRayTracing            = binaryMetadata.rayQueryUsed;
#endif
            objectCreateInfo.flags.isPointSizeUsed          = binaryMetadata.pointSizeUsed;
            objectCreateInfo.flags.shadingRateUsedInShader  = binaryMetadata.shadingRateUsedInShader;
            objectCreateInfo.flags.viewIndexFromDeviceIndex = Util::TestAnyFlagSet(flags,
                VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT);

#if VKI_RAY_TRACING
            objectCreateInfo.dispatchRaysUserDataOffset = pPipelineLayout->GetDispatchRaysUserData();
#endif

            // 7. Create pipeline objects
            result = CreatePipelineObjects(
                pDevice,
                pCreateInfo,
                flags,
                pAllocator,
                pPipelineLayout,
                &binaryMetadata.vbInfo,
                &binaryMetadata.internalBufferInfo,
                pipelineBinarySizes,
                pPipelineBinaries,
                pPipelineCache,
                cacheId,
                apiPsoHash,
                &objectCreateInfo,
                pPipeline);
        }
    }

    // Free the temporary newly-built shader modules
    FreeTempModules(pDevice, ShaderStage::ShaderStageGfxCount, tempModules);

    // Free the allocate space for shader library pointers
    if (objectCreateInfo.pipeline.ppShaderLibraries != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, objectCreateInfo.pipeline.ppShaderLibraries);
    }

    if (binaryMetadata.internalBufferInfo.pData != nullptr)
    {
        pDevice->VkInstance()->FreeMem(binaryMetadata.internalBufferInfo.pData);
        binaryMetadata.internalBufferInfo.pData = nullptr;
    }
    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            pDevice->GetCompiler(deviceIdx)->FreeGraphicsPipelineBinary(
                binaryCreateInfo.compilerType,
                binaryCreateInfo.freeCompilerBinary,
                pPipelineBinaries[deviceIdx],
                pipelineBinarySizes[deviceIdx]);
        }
    }

    // Deferred compile will reuse all object generated in PipelineCompiler::ConvertGraphicsPipelineInfo.
    // i.e. we need keep temp buffer in binaryCreateInfo
    pDefaultCompiler->FreeGraphicsPipelineCreateInfo(&binaryCreateInfo,
                                                     objectCreateInfo.immedInfo.checkDeferCompilePipeline);

    if (objectCreateInfo.immedInfo.checkDeferCompilePipeline)
    {
        GraphicsPipeline* pThis = GraphicsPipeline::ObjectFromHandle(*pPipeline);
        result = pThis->BuildDeferCompileWorkload(pDevice,
                                                  pPipelineCache,
                                                  &binaryCreateInfo,
                                                  &shaderStageInfo,
                                                  &objectCreateInfo,
                                                  &elfHash);
        if (result == VK_SUCCESS)
        {
            pDefaultCompiler->ExecuteDeferCompile(&pThis->m_deferWorkload);
        }
    }

    if (result == VK_SUCCESS)
    {
        const Device::DeviceFeatures& deviceFeatures = pDevice->GetEnabledFeatures();

        uint64_t durationTicks = Util::GetPerfCpuTime() - startTimeTicks;
        uint64_t duration = vk::utils::TicksToNano(durationTicks);
        binaryCreateInfo.pipelineFeedback.feedbackValid = true;
        binaryCreateInfo.pipelineFeedback.duration = duration;
        pDefaultCompiler->SetPipelineCreationFeedbackInfo(
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

        // The hash is same as pipline dump file name, we can easily analyze further.
        AmdvlkLog(pDevice->GetRuntimeSettings().logTagIdMask,
                  PipelineCompileTime,
                  "0x%016llX-%llu",
                  apiPsoHash, duration);
    }

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
VkResult GraphicsPipeline::BuildDeferCompileWorkload(
    Device*                           pDevice,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
    GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
    Util::MetroHash::Hash*            pElfHash)
{
    VkResult result = VK_SUCCESS;
    DeferGraphicsPipelineCreateInfo* pCreateInfo = nullptr;

    // Calculate payload size
    size_t payloadSize = sizeof(DeferGraphicsPipelineCreateInfo) + sizeof(Util::Event);
    for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; i++)
    {
        if (pShaderStageInfo->stages[i].pEntryPoint != nullptr)
        {
            payloadSize += strlen(pShaderStageInfo->stages[i].pEntryPoint) + 1;
            if (pShaderStageInfo->stages[i].pSpecializationInfo != nullptr)
            {
                auto pSpecializationInfo = pShaderStageInfo->stages[i].pSpecializationInfo;
                payloadSize += sizeof(VkSpecializationInfo);
                payloadSize += sizeof(VkSpecializationMapEntry) * pSpecializationInfo->mapEntryCount;
                payloadSize += pSpecializationInfo->dataSize;
            }
        }
    }

    size_t vertexInputSize = 0;
    if ((pShaderStageInfo->stages[ShaderStage::ShaderStageVertex].pEntryPoint != nullptr) &&
        (pBinaryCreateInfo->pipelineInfo.pVertexInput != nullptr))
    {
        vertexInputSize =  GetVertexInputStructSize(pBinaryCreateInfo->pipelineInfo.pVertexInput);
        payloadSize += vertexInputSize;
    }

    size_t memOffset = 0;
    Instance* pInstance = pDevice->VkInstance();
    void* pPayloadMem = pInstance->AllocMem(payloadSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pPayloadMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        memset(pPayloadMem, 0, payloadSize);
        pCreateInfo = static_cast<DeferGraphicsPipelineCreateInfo*>(pPayloadMem);
        memOffset = sizeof(DeferGraphicsPipelineCreateInfo);

        // Fill create info and reset defer compile related options
        pCreateInfo->pDevice          = pDevice;
        pCreateInfo->pPipelineCache   = pPipelineCache;
        pCreateInfo->pPipeline        = this;
        pCreateInfo->shaderStageInfo  = *pShaderStageInfo;
        pCreateInfo->binaryCreateInfo = *pBinaryCreateInfo;
        pCreateInfo->objectCreateInfo = *pObjectCreateInfo;
        pCreateInfo->elfHash          = *pElfHash;

        pCreateInfo->binaryCreateInfo.pipelineInfo.enableEarlyCompile = false;
        pCreateInfo->binaryCreateInfo.pipelineInfo.enableUberFetchShader = false;
        pCreateInfo->objectCreateInfo.immedInfo.checkDeferCompilePipeline = false;

        PipelineShaderInfo* pShaderInfo[] =
        {
            &pCreateInfo->binaryCreateInfo.pipelineInfo.task,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.vs,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.tcs,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.tes,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.gs,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.mesh,
            &pCreateInfo->binaryCreateInfo.pipelineInfo.fs,
        };

        // Do deep copy for binaryCreateInfo members
        for (uint32_t i = 0; i < ShaderStage::ShaderStageGfxCount; i++)
        {
            if (pShaderStageInfo->stages[i].pEntryPoint != nullptr)
            {
                size_t size = strlen(pShaderStageInfo->stages[i].pEntryPoint) + 1;
                char* pEntryPoint = reinterpret_cast<char*>(Util::VoidPtrInc(pPayloadMem, memOffset));
                memcpy(pEntryPoint, pShaderStageInfo->stages[i].pEntryPoint, size);
                pCreateInfo->shaderStageInfo.stages[i].pEntryPoint = pEntryPoint;
                pShaderInfo[i]->pEntryTarget = pEntryPoint;
                memOffset += size;

                if (pShaderStageInfo->stages[i].pSpecializationInfo != nullptr)
                {
                    auto pSrcSpecInfo = pShaderStageInfo->stages[i].pSpecializationInfo;
                    auto pDestSpecInfo = reinterpret_cast<VkSpecializationInfo*>(Util::VoidPtrInc(pPayloadMem, memOffset));
                    *pDestSpecInfo = *pSrcSpecInfo;
                    memOffset += sizeof(VkSpecializationInfo);

                    pDestSpecInfo->pMapEntries = reinterpret_cast<VkSpecializationMapEntry*>(Util::VoidPtrInc(pPayloadMem, memOffset));
                    memcpy(const_cast<VkSpecializationMapEntry*>(pDestSpecInfo->pMapEntries),
                           pSrcSpecInfo->pMapEntries,
                           pSrcSpecInfo->mapEntryCount * sizeof(VkSpecializationMapEntry));
                    memOffset += pSrcSpecInfo->mapEntryCount * sizeof(VkSpecializationMapEntry);

                    pDestSpecInfo->pData = Util::VoidPtrInc(pPayloadMem, memOffset);
                    memcpy(const_cast<void*>(pDestSpecInfo->pData),
                           pSrcSpecInfo->pData,
                           pSrcSpecInfo->dataSize);
                    memOffset += pSrcSpecInfo->dataSize;
                    pCreateInfo->shaderStageInfo.stages[i].pSpecializationInfo = pDestSpecInfo;
                    pShaderInfo[i]->pSpecializationInfo = pDestSpecInfo;
                }
            }
        }

        if (vertexInputSize != 0)
        {
            VkPipelineVertexInputStateCreateInfo* pVertexInput =
                reinterpret_cast<VkPipelineVertexInputStateCreateInfo*>(Util::VoidPtrInc(pPayloadMem, memOffset));
            pCreateInfo->binaryCreateInfo.pipelineInfo.pVertexInput = pVertexInput;
            CopyVertexInputStruct(pBinaryCreateInfo->pipelineInfo.pVertexInput, pVertexInput);
            memOffset += vertexInputSize;
        }

        // Copy pipeline optimizer key
        memcpy(
            pCreateInfo->shaderOptimizerKeys,
            pBinaryCreateInfo->pPipelineProfileKey->pShaders,
            sizeof(ShaderOptimizerKey)* pBinaryCreateInfo->pPipelineProfileKey->shaderCount);
        pCreateInfo->pipelineOptimizerKey.pShaders        = pCreateInfo->shaderOptimizerKeys;
        pCreateInfo->pipelineOptimizerKey.shaderCount     = pBinaryCreateInfo->pPipelineProfileKey->shaderCount;
        pCreateInfo->binaryCreateInfo.pPipelineProfileKey = &pCreateInfo->pipelineOptimizerKey;

        // Copy binary metadata
        pCreateInfo->binaryMetadata                   = *pBinaryCreateInfo->pBinaryMetadata;
        pCreateInfo->binaryCreateInfo.pBinaryMetadata = &pCreateInfo->binaryMetadata;

        // Build defer workload
        m_deferWorkload.pPayloads = pPayloadMem;
        m_deferWorkload.pEvent = VK_PLACEMENT_NEW(Util::VoidPtrInc(pPayloadMem, memOffset))(Util::Event);
        memOffset += sizeof(Util::Event);
        VK_ASSERT(memOffset == payloadSize);

        EventCreateFlags  flags = {};
        flags.manualReset = true;
        m_deferWorkload.pEvent->Init(flags);
        m_deferWorkload.Execute = ExecuteDeferCreateOptimizedPipeline;
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipeline::ExecuteDeferCreateOptimizedPipeline(
    void *pPayload)
{
    DeferGraphicsPipelineCreateInfo* pCreateInfo = static_cast<DeferGraphicsPipelineCreateInfo*>(pPayload);
    pCreateInfo->pPipeline->DeferCreateOptimizedPipeline(pCreateInfo->pDevice,
                                                         pCreateInfo->pPipelineCache,
                                                         &pCreateInfo->binaryCreateInfo,
                                                         &pCreateInfo->shaderStageInfo,
                                                         &pCreateInfo->objectCreateInfo,
                                                         &pCreateInfo->elfHash);
}

// =====================================================================================================================
VkResult GraphicsPipeline::DeferCreateOptimizedPipeline(
    Device*                           pDevice,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
    GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
    Util::MetroHash::Hash*            pElfHash)
{
    VkResult              result = VK_SUCCESS;
    size_t                pipelineBinarySizes[MaxPalDevices] = {};
    const void*           pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash cacheId[MaxPalDevices]             = {};
    Pal::IPipeline*       pPalPipeline[MaxPalDevices]        = {};

    Pal::Result           palResult                          = Pal::Result::Success;
    size_t palSize =
        pDevice->PalDevice(DefaultDeviceIndex)->GetGraphicsPipelineSize(pObjectCreateInfo->pipeline, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    uint32_t numPalDevices = pDevice->NumPalDevices();
    void* pSystemMem = pDevice->VkInstance()->AllocMem(
        palSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
    if (pSystemMem == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (result == VK_SUCCESS)
    {
        result = CreatePipelineBinaries(pDevice,
                                        nullptr,
                                        0,
                                        pShaderStageInfo,
                                        nullptr,
                                        pElfHash,
                                        pBinaryCreateInfo->pPipelineProfileKey,
                                        pBinaryCreateInfo,
                                        pPipelineCache,
                                        nullptr,
                                        cacheId,
                                        pipelineBinarySizes,
                                        pPipelineBinaries,
                                        pBinaryCreateInfo->pBinaryMetadata);
    }

    if (result == VK_SUCCESS)
    {
        result = CreatePalPipelineObjects(pDevice,
                                          pPipelineCache,
                                          pObjectCreateInfo,
                                          pipelineBinarySizes,
                                          pPipelineBinaries,
                                          cacheId,
                                          pSystemMem,
                                          pPalPipeline);
    }

    if (result == VK_SUCCESS)
    {
        VK_ASSERT(pSystemMem == pPalPipeline[0]);
        SetOptimizedPipeline(pPalPipeline);
    }

    pDevice->GetCompiler(DefaultDeviceIndex)->FreeGraphicsPipelineCreateInfo(pBinaryCreateInfo, false);

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            pDevice->GetCompiler(deviceIdx)->FreeGraphicsPipelineBinary(
                pBinaryCreateInfo->compilerType,
                pBinaryCreateInfo->freeCompilerBinary,
                pPipelineBinaries[deviceIdx],
                pipelineBinarySizes[deviceIdx]);
        }
    }
    return result;
}

// =====================================================================================================================
VkResult GraphicsPipeline::PrepareShaderLibrary(
    Device*                           pDevice,
    const VkAllocationCallbacks*      pAllocator,
    GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo)
{
    VkResult result = VK_SUCCESS;
    // Copy shader libraries to object create info if all necessary libraries were ready
    if ((pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryPreRaster] != nullptr) &&
        (pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryFragment]  != nullptr))
    {
        VK_ASSERT(pObjectCreateInfo->pipeline.ppShaderLibraries == nullptr);

        size_t bufSize = sizeof(Pal::IShaderLibrary*) * GraphicsLibraryCount;
        void* pTempBuf = pAllocator->pfnAllocation(pAllocator->pUserData,
                                                   bufSize,
                                                   VK_DEFAULT_MEM_ALIGN,
                                                   VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
        if (pTempBuf != nullptr)
        {
            memset(pTempBuf, 0, bufSize);
            Pal::IShaderLibrary** ppShaderLibraries     = static_cast<Pal::IShaderLibrary**>(pTempBuf);
            ppShaderLibraries[GraphicsLibraryPreRaster] =
                pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryPreRaster];
            ppShaderLibraries[GraphicsLibraryFragment]  =
                pBinaryCreateInfo->pShaderLibraries[GraphicsLibraryFragment];

            pObjectCreateInfo->pipeline.numShaderLibraries = 2;

            if (pBinaryCreateInfo->pipelineInfo.enableColorExportShader)
            {
                result = pDevice->GetCompiler(DefaultDeviceIndex)->
                    CreateColorExportShaderLibrary(pDevice,
                        pBinaryCreateInfo,
                        pAllocator,
                        &ppShaderLibraries[GraphicsLibraryColorExport]);

                if (result == VK_SUCCESS)
                {
                    pObjectCreateInfo->pipeline.numShaderLibraries += 1;
                }
            }

            if (result == VK_SUCCESS)
            {
                pObjectCreateInfo->pipeline.ppShaderLibraries =
                    const_cast<const Pal::IShaderLibrary**>(ppShaderLibraries);
            }
            else if (result == VK_PIPELINE_COMPILE_REQUIRED)
            {
                // Not applicable for shader library linking.
                // Free allocated memory and reset the number of shader libraries to 0
                pAllocator->pfnFree(pAllocator->pUserData, pTempBuf);
                pObjectCreateInfo->pipeline.numShaderLibraries = 0;
                result = VK_SUCCESS;
            }
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return result;
}

// =====================================================================================================================
void GraphicsPipeline::SetOptimizedPipeline(
    Pal::IPipeline* pPalPipeline[MaxPalDevices])
{
    const bool optimizedPipeline = true;
    Util::MetroHash::Hash hash = {};
    Util::MetroHash64 palPipelineHasher;
    palPipelineHasher.Update(PalPipelineHash());
    palPipelineHasher.Update(optimizedPipeline);
    palPipelineHasher.Finalize(hash.bytes);

    Util::MutexAuto pipelineSwitchLock(&m_pipelineSwitchLock);
    memcpy(m_pOptimizedPipeline, pPalPipeline, sizeof(m_pOptimizedPipeline));
    m_optimizedPipelineHash = hash.qwords[0];
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* const                          pDevice,
    Pal::IPipeline**                       pPalPipeline,
    const PipelineLayout*                  pLayout,
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
    m_vbInfo(vbInfo),
    m_internalBufferInfo(*pInternalBuffer),
    m_pOptimizedPipeline{},
    m_optimizedPipelineHash(0),
    m_deferWorkload{},
    m_flags(flags)
{
    Pipeline::Init(
        pPalPipeline,
        pLayout,
        staticStateMask,
#if VKI_RAY_TRACING
        dispatchRaysUserDataOffset,
#endif
        cacheHash,
        apiHash);

    memcpy(m_pPalMsaa,         pPalMsaa,         sizeof(pPalMsaa[0])         * pDevice->NumPalDevices());
    memcpy(m_pPalColorBlend,   pPalColorBlend,   sizeof(pPalColorBlend[0])   * pDevice->NumPalDevices());
    memcpy(m_pPalDepthStencil, pPalDepthStencil, sizeof(pPalDepthStencil[0]) * pDevice->NumPalDevices());

    CreateStaticState();

    if (ContainsDynamicState(DynamicStatesInternal::RasterizerDiscardEnable))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.rasterizerDiscardEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::ColorWriteEnable) ||
        ContainsDynamicState(DynamicStatesInternal::ColorWriteMask))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.colorWriteMask = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::LogicOp) ||
        ContainsDynamicState(DynamicStatesInternal::LogicOpEnable))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.logicOp = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::LineRasterizationMode))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.perpLineEndCapsEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::TessellationDomainOrigin))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.switchWinding = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::AlphaToCoverageEnable))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.alphaToCoverageEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClipNegativeOneToOne))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.depthRange = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClipEnable))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.depthClipMode = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::DepthClampEnable))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.depthClampMode = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::ColorBlendEquation))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.dualSourceBlendEnable = 1;
    }

    if (ContainsDynamicState(DynamicStatesInternal::VertexInput))
    {
        m_info.graphicsShaderInfos.dynamicState.enable.vertexBufferCount = 1;
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
    if (m_deferWorkload.pEvent != nullptr)
    {
        auto result = m_deferWorkload.pEvent->Wait(10);
        if (result == Util::Result::Success)
        {
            Util::Destructor(m_deferWorkload.pEvent);
            pDevice->VkInstance()->FreeMem(m_deferWorkload.pPayloads);
        }
        m_deferWorkload.pEvent = nullptr;
        m_deferWorkload.pPayloads = nullptr;
    }

    DestroyStaticState(pAllocator);

    if (m_pOptimizedPipeline[0] != nullptr)
    {
        void* pBaseMem = m_pOptimizedPipeline[0];
        for (uint32_t deviceIdx = 0;
            (deviceIdx < m_pDevice->NumPalDevices()) && (m_pPalPipeline[deviceIdx] != nullptr);
            deviceIdx++)
        {
            m_pOptimizedPipeline[deviceIdx]->Destroy();
            m_pOptimizedPipeline[deviceIdx] = nullptr;
        }
        pDevice->VkInstance()->FreeMem(pBaseMem);
    }

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
// Binds this graphics pipeline's state to the given command buffer (with passed in wavelimits)
void GraphicsPipeline::BindToCmdBuffer(
    CmdBuffer*                             pCmdBuffer
    ) const
{
    AllGpuRenderState* pRenderState = pCmdBuffer->RenderState();

    const Pal::DynamicGraphicsShaderInfos& graphicsShaderInfos = m_info.graphicsShaderInfos;
    Pal::DynamicGraphicsShaderInfos*       pGfxDynamicBindInfo =
        &pRenderState->pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx;

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

        Pal::DepthRange depthRange = pGfxDynamicBindInfo->dynamicState.depthRange;
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

    const bool useOptimizedPipeline = UseOptimizedPipeline();
    const uint64_t oldHash = pRenderState->boundGraphicsPipelineHash;
    const uint64_t newHash = useOptimizedPipeline ? m_optimizedPipelineHash : PalPipelineHash();
    const bool dynamicStateDirty =
        pGfxDynamicBindInfo->dynamicState.enable.u32All != graphicsShaderInfos.dynamicState.enable.u32All;

    // Update pipleine dynamic state
    pGfxDynamicBindInfo->dynamicState.enable.u32All = graphicsShaderInfos.dynamicState.enable.u32All;

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

        pGfxDynamicBindInfo->dynamicState.colorWriteMask =
            pRenderState->colorWriteMask & pRenderState->colorWriteEnable;
    }

    // Overwrite state with dynamic state in current render state
    if (graphicsShaderInfos.dynamicState.enable.logicOp)
    {
        if (ContainsStaticState(DynamicStatesInternal::LogicOp))
        {
            pRenderState->logicOp = m_info.logicOp;
            pGfxDynamicBindInfo->dynamicState.logicOp =
                pRenderState->logicOpEnable ? VkToPalLogicOp(pRenderState->logicOp) : Pal::LogicOp::Copy;
        }

        if (ContainsStaticState(DynamicStatesInternal::LogicOpEnable))
        {
            pRenderState->logicOpEnable = m_info.logicOpEnable;
            pGfxDynamicBindInfo->dynamicState.logicOp =
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

        uint32_t debugPrintfRegBase = (m_userDataLayout.scheme == PipelineLayoutScheme::Compact) ?
            m_userDataLayout.compact.debugPrintfRegBase : m_userDataLayout.indirect.debugPrintfRegBase;
        pCmdBuffer->GetDebugPrintf()->BindPipeline(m_pDevice,
                                                   this,
                                                   deviceIdx,
                                                   pPalCmdBuf,
                                                   static_cast<uint32_t>(Pal::PipelineBindPoint::Graphics),
                                                   debugPrintfRegBase);
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
            ContainsStaticState(DynamicStatesInternal::SampleLocationsEnable))
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
                if (memcmp(&pRenderState->samplePattern, &m_info.samplePattern, sizeof(SamplePattern)) != 0)
                {
                    pRenderState->samplePattern = m_info.samplePattern;
                    pRenderState->dirtyGraphics.samplePattern = 1;
                }
            }
            else if (ContainsStaticState(DynamicStatesInternal::SampleLocationsEnable))
            {
                if (pRenderState->sampleLocationsEnable != m_flags.customSampleLocations)
                {
                    pRenderState->sampleLocationsEnable = m_flags.customSampleLocations;
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

        if ((useOptimizedPipeline == false) && (m_internalBufferInfo.dataSize > 0))
        {
            VK_ASSERT(m_internalBufferInfo.internalBufferCount > 0);
            Pal::gpusize gpuAddress = {};
            uint32_t* pCpuAddr = pPalCmdBuf->CmdAllocateEmbeddedData(m_internalBufferInfo.dataSize, 1, &gpuAddress);
            memcpy(pCpuAddr, m_internalBufferInfo.pData, m_internalBufferInfo.dataSize);
            for (uint32_t i = 0; i < m_internalBufferInfo.internalBufferCount; i++)
            {
                Pal::gpusize bufferAddress = gpuAddress;
                bufferAddress += m_internalBufferInfo.internalBufferEntries[i].bufferOffset;
                pPalCmdBuf->CmdSetUserData(Pal::PipelineBindPoint::Graphics,
                    m_internalBufferInfo.internalBufferEntries[i].userDataOffset,
                    2,
                    reinterpret_cast<uint32_t*>(&bufferAddress));
            }
        }
    }
    while (deviceGroup.IterateNext());

    pRenderState->boundGraphicsPipelineHash = newHash;

    // Binding GraphicsPipeline affects ViewMask,
    // because when VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT is specified
    // ViewMask for each VkPhysicalDevice is defined by DeviceIndex
    // not by current subpass during a render pass instance.
    const bool oldViewIndexFromDeviceIndex = pRenderState->viewIndexFromDeviceIndex;
    const bool newViewIndexFromDeviceIndex = ViewIndexFromDeviceIndex();

    if (oldViewIndexFromDeviceIndex != newViewIndexFromDeviceIndex)
    {
        // Update value of ViewIndexFromDeviceIndex for currently bound pipeline.
        pRenderState->viewIndexFromDeviceIndex = newViewIndexFromDeviceIndex;

        // Sync ViewMask state in CommandBuffer.
        pCmdBuffer->SetViewInstanceMask(pCmdBuffer->GetDeviceMask());
    }
}

// =====================================================================================================================
// Binds a null pipeline to PAL
void GraphicsPipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numDevices = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
    params.apiPsoHash = Pal::InternalApiPsoHash;

    for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
    {
        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        pPalCmdBuf->CmdBindPipeline(params);
        pPalCmdBuf->CmdBindMsaaState(nullptr);
        pPalCmdBuf->CmdBindColorBlendState(nullptr);
        pPalCmdBuf->CmdBindDepthStencilState(nullptr);
    }
}
} // namespace vk
