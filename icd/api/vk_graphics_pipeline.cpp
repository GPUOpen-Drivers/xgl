/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
    const GraphicsPipelineShaderStageInfo*         pShaderInfo,
    const PipelineLayout*                          pPipelineLayout,
    GraphicsPipelineBinaryCreateInfo*              pBinaryCreateInfo,
    PipelineCache*                                 pPipelineCache,
    const VkPipelineCreationFeedbackCreateInfoEXT* pCreationFeedbackInfo,
    Util::MetroHash::Hash*                         pCacheIds,
    size_t*                                        pPipelineBinarySizes,
    const void**                                   pPipelineBinaries)
{
    VkResult          result = VK_SUCCESS;
    const uint32_t    numPalDevices = pDevice->NumPalDevices();
    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    for (uint32_t i = 0; (result == VK_SUCCESS) && (i < numPalDevices)
         ; ++i)
    {
        if ((i == DefaultDeviceIndex) || (pCreateInfo == nullptr))
        {
            result = pDevice->GetCompiler(i)->CreateGraphicsPipelineBinary(
                pDevice,
                i,
                pPipelineCache,
                pBinaryCreateInfo,
                &pPipelineBinarySizes[i],
                &pPipelineBinaries[i],
                &pCacheIds[i]);
        }
        else
        {
            GraphicsPipelineBinaryCreateInfo binaryCreateInfoMGPU = {};
            VbBindingInfo vbInfoMGPU = {};
            PipelineInternalBufferInfo internalBufferInfoMGPU = {};
            pDefaultCompiler->ConvertGraphicsPipelineInfo(
                pDevice, pCreateInfo, pShaderInfo, pPipelineLayout, &binaryCreateInfoMGPU, &vbInfoMGPU, &internalBufferInfoMGPU);

            result = pDevice->GetCompiler(i)->CreateGraphicsPipelineBinary(
                pDevice,
                i,
                pPipelineCache,
                &binaryCreateInfoMGPU,
                &pPipelineBinarySizes[i],
                &pPipelineBinaries[i],
                &pCacheIds[i]);

            if (result == VK_SUCCESS)
            {
                pDefaultCompiler->SetPipelineCreationFeedbackInfo(
                    pCreationFeedbackInfo,
                    pCreateInfo->stageCount,
                    pCreateInfo->pStages,
                    &binaryCreateInfoMGPU.pipelineFeedback,
                    binaryCreateInfoMGPU.stageFeedback);
            }

            pDefaultCompiler->FreeGraphicsPipelineCreateInfo(&binaryCreateInfoMGPU, false);
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
    pObjectCreateInfo->pipeline.pPipelineBinary = pPipelineBinaries[DefaultDeviceIndex];

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
                pObjectCreateInfo->pipeline.pPipelineBinary = pPipelineBinaries[deviceIdx];
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
    const VkAllocationCallbacks*        pAllocator,
    const PipelineLayout*               pPipelineLayout,
    const VbBindingInfo*                pVbInfo,
    const PipelineInternalBufferInfo*   pInternalBuffer,
    const size_t*                       pPipelineBinarySizes,
    const void**                        pPipelineBinaries,
    PipelineCache*                      pPipelineCache,
    const Util::MetroHash::Hash*        pCacheIds,
    GraphicsPipelineObjectCreateInfo*   pObjectCreateInfo,
    VkPipeline*                         pPipeline)
{
    VkResult          result        = VK_SUCCESS;
    Pal::Result       palResult     = Pal::Result::Success;
    const uint32_t    numPalDevices = pDevice->NumPalDevices();
    uint64_t          apiPsoHash    = 0;
    Util::MetroHash64 palPipelineHasher;

    apiPsoHash = BuildApiHash(pCreateInfo, pObjectCreateInfo);
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
        for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

            // Create the PAL MSAA state object
            if (palResult == Pal::Result::Success)
            {
                // Force full sample shading if the app didn't enable it, but the shader wants
                // per-sample shading by the use of SampleId or similar features.
                if ((pObjectCreateInfo->immedInfo.rasterizerDiscardEnable != VK_TRUE) &&
                    (pObjectCreateInfo->flags.sampleShadingEnable == false))
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
                        pObjectCreateInfo->msaa.pixelShaderSamples  = pObjectCreateInfo->msaa.coverageSamples;
                    }
                }

                palResult = pRSCache->CreateMsaaState(
                    pObjectCreateInfo->msaa,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalMsaa);
            }

            // Create the PAL color blend state object
            if (palResult == Pal::Result::Success)
            {
                palResult = pRSCache->CreateColorBlendState(
                    pObjectCreateInfo->blend,
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

    PipelineBinaryInfo* pBinaryInfo = nullptr;

    if ((pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) ||
        (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_PIPELINE_EXECUTABLE_PROPERTIES) &&
        ((pCreateInfo->flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) != 0))) &&
        (result == VK_SUCCESS))
    {
        pBinaryInfo = PipelineBinaryInfo::Create(
            pPipelineBinarySizes[DefaultDeviceIndex],
            pPipelineBinaries[DefaultDeviceIndex],
            pAllocator);
    }

    // On success, wrap it up in a Vulkan object.
    if (result == VK_SUCCESS)
    {
        const bool viewIndexFromDeviceIndex = Util::TestAnyFlagSet(
            pCreateInfo->flags,
            VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT);

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
            pObjectCreateInfo->flags.bindDepthStencilObject,
            pObjectCreateInfo->flags.bindTriangleRasterState,
            pObjectCreateInfo->flags.bindStencilRefMasks,
            pObjectCreateInfo->flags.bindInputAssemblyState,
            pObjectCreateInfo->flags.force1x1ShaderRate,
            pObjectCreateInfo->flags.customSampleLocations,
            pObjectCreateInfo->flags.isPointSizeUsed,
#if VKI_RAY_TRACING
            pObjectCreateInfo->flags.hasRayTracing,
            pObjectCreateInfo->dispatchRaysUserDataOffset,
#endif
            *pVbInfo,
            &internalBuffer,
            pPalMsaa,
            pPalColorBlend,
            pPalDepthStencil,
            pObjectCreateInfo->sampleCoverage,
            viewIndexFromDeviceIndex,
            pBinaryInfo,
            apiPsoHash,
            &palPipelineHasher);

        *pPipeline = GraphicsPipeline::HandleFromVoidPointer(pSystemMem);
    }

    if (result != VK_SUCCESS)
    {
        pRSCache->DestroyMsaaState(pPalMsaa, pAllocator);
        pRSCache->DestroyColorBlendState(pPalColorBlend, pAllocator);

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

        if (pBinaryInfo != nullptr)
        {
            pBinaryInfo->Destroy(pAllocator);
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
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    uint64 startTimeTicks = Util::GetPerfCpuTime();

    PipelineCompiler* pDefaultCompiler = pDevice->GetCompiler(DefaultDeviceIndex);

    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfo = nullptr;

    pDefaultCompiler->GetPipelineCreationFeedback(static_cast<const VkStructHeader*>(pCreateInfo->pNext),
                                                  &pPipelineCreationFeedbackCreateInfo);

    // 1. Get pipeline layout
    VK_ASSERT(pCreateInfo->layout != VK_NULL_HANDLE);
    PipelineLayout* pPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);

    // 2. Build pipeline binary create info
    GraphicsPipelineBinaryCreateInfo binaryCreateInfo  = {};
    GraphicsPipelineShaderStageInfo  shaderStageInfo   = {};
    VbBindingInfo                    vbInfo            = {};
    PipelineInternalBufferInfo       internalBufferInfo = {};
    ShaderModuleHandle               tempModules[ShaderStage::ShaderStageGfxCount] = {};

    VkResult result = BuildPipelineBinaryCreateInfo(
        pDevice,
        pCreateInfo,
        pPipelineLayout,
        pPipelineCache,
        &binaryCreateInfo,
        &shaderStageInfo,
        &vbInfo,
        &internalBufferInfo,
        tempModules);

    // 3. Create pipeine binaries
    size_t                pipelineBinarySizes[MaxPalDevices] = {};
    const void*           pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash cacheId[MaxPalDevices]             = {};

    if (result == VK_SUCCESS)
    {
        result = CreatePipelineBinaries(pDevice,
                                        pCreateInfo,
                                        &shaderStageInfo,
                                        pPipelineLayout,
                                        &binaryCreateInfo,
                                        pPipelineCache,
                                        pPipelineCreationFeedbackCreateInfo,
                                        cacheId,
                                        pipelineBinarySizes,
                                        pPipelineBinaries);
    }

    uint64_t pipelineHash = 0;
    GraphicsPipelineObjectCreateInfo objectCreateInfo = {};
    GraphicsPipelineBinaryInfo       binaryInfo       = {};
    if (result == VK_SUCCESS)
    {
        pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&binaryCreateInfo.pipelineInfo);

        // 4. Build pipeline object create info
        binaryInfo.pOptimizerKey = &binaryCreateInfo.pipelineProfileKey;
#if VKI_RAY_TRACING
        binaryInfo.hasRayTracing = binaryCreateInfo.pipelineInfo.rtState.threadGroupSizeX > 0;
#endif
        binaryInfo.hasMesh       = binaryCreateInfo.pipelineInfo.mesh.pModuleData != nullptr;

        BuildPipelineObjectCreateInfo(
            pDevice, pCreateInfo, &vbInfo, &binaryInfo, pPipelineLayout, &objectCreateInfo);

        objectCreateInfo.immedInfo.checkDeferCompilePipeline =
            pDevice->GetRuntimeSettings().deferCompileOptimizedPipeline &&
            (binaryCreateInfo.pipelineInfo.enableEarlyCompile || binaryCreateInfo.pipelineInfo.enableUberFetchShader);

        objectCreateInfo.flags.isPointSizeUsed = binaryCreateInfo.pipelineMetadata.pointSizeUsed;

#if VKI_RAY_TRACING
        objectCreateInfo.dispatchRaysUserDataOffset = pPipelineLayout->GetDispatchRaysUserData();
#endif

        // 5. Create pipeline objects
        result = CreatePipelineObjects(
            pDevice,
            pCreateInfo,
            pAllocator,
            pPipelineLayout,
            &vbInfo,
            &internalBufferInfo,
            pipelineBinarySizes,
            pPipelineBinaries,
            pPipelineCache,
            cacheId,
            &objectCreateInfo,
            pPipeline);
    }

    // Free the temporary newly-built shader modules
    FreeTempModules(pDevice, ShaderStage::ShaderStageGfxCount, tempModules);

    if (internalBufferInfo.pData != nullptr)
    {
        pDevice->VkInstance()->FreeMem(internalBufferInfo.pData);
        internalBufferInfo.pData = nullptr;
    }
    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            pDevice->GetCompiler(deviceIdx)->FreeGraphicsPipelineBinary(
                &binaryCreateInfo, pPipelineBinaries[deviceIdx], pipelineBinarySizes[deviceIdx]);
        }
    }

    // Deferred compile will reuse all object generated in BuildPipelineBinaryCreateInfo.
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
                                                  &objectCreateInfo);
        if (result == VK_SUCCESS)
        {
            pDefaultCompiler->ExecuteDeferCompile(&pThis->m_deferWorkload);
        }
    }

    if (result == VK_SUCCESS)
    {
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

        // The hash is same as pipline dump file name, we can easily analyze further.
        AmdvlkLog(pDevice->GetRuntimeSettings().logTagIdMask,
                  PipelineCompileTime,
                  "0x%016llX-%llu",
                  pipelineHash, duration);
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
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo)
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
                                                         &pCreateInfo->objectCreateInfo);
}

// =====================================================================================================================
VkResult GraphicsPipeline::DeferCreateOptimizedPipeline(
    Device*                           pDevice,
    PipelineCache*                    pPipelineCache,
    GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
    GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
    GraphicsPipelineObjectCreateInfo* pObjectCreateInfo)
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
                                        pShaderStageInfo,
                                        nullptr,
                                        pBinaryCreateInfo,
                                        pPipelineCache,
                                        nullptr,
                                        cacheId,
                                        pipelineBinarySizes,
                                        pPipelineBinaries);
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
                pBinaryCreateInfo, pPipelineBinaries[deviceIdx], pipelineBinarySizes[deviceIdx]);
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
    bool                                   bindDepthStencilObject,
    bool                                   bindTriangleRasterState,
    bool                                   bindStencilRefMasks,
    bool                                   bindInputAssemblyState,
    bool                                   force1x1ShaderRate,
    bool                                   customSampleLocations,
    bool                                   isPointSizeUsed,
#if VKI_RAY_TRACING
    bool                                   hasRayTracing,
    uint32_t                               dispatchRaysUserDataOffset,
#endif
    const VbBindingInfo&                   vbInfo,
    const PipelineInternalBufferInfo*      pInternalBuffer,
    Pal::IMsaaState**                      pPalMsaa,
    Pal::IColorBlendState**                pPalColorBlend,
    Pal::IDepthStencilState**              pPalDepthStencil,
    uint32_t                               coverageSamples,
    bool                                   viewIndexFromDeviceIndex,
    PipelineBinaryInfo*                    pBinary,
    uint64_t                               apiHash,
    Util::MetroHash64*                     pPalPipelineHasher)
    :
    GraphicsPipelineCommon(
#if VKI_RAY_TRACING
        hasRayTracing,
#endif
        pDevice),
    m_info(immedInfo),
    m_vbInfo(vbInfo),
    m_internalBufferInfo(*pInternalBuffer),
    m_pOptimizedPipeline{},
    m_optimizedPipelineHash(0),
    m_deferWorkload{},
    m_flags()
{
    Pipeline::Init(
        pPalPipeline,
        pLayout,
        pBinary,
        staticStateMask,
#if VKI_RAY_TRACING
        dispatchRaysUserDataOffset,
#endif
        apiHash);

    memcpy(m_pPalMsaa,         pPalMsaa,         sizeof(pPalMsaa[0])         * pDevice->NumPalDevices());
    memcpy(m_pPalColorBlend,   pPalColorBlend,   sizeof(pPalColorBlend[0])   * pDevice->NumPalDevices());
    memcpy(m_pPalDepthStencil, pPalDepthStencil, sizeof(pPalDepthStencil[0]) * pDevice->NumPalDevices());

    m_flags.viewIndexFromDeviceIndex = viewIndexFromDeviceIndex;
    m_flags.bindDepthStencilObject   = bindDepthStencilObject;
    m_flags.bindTriangleRasterState  = bindTriangleRasterState;
    m_flags.bindStencilRefMasks      = bindStencilRefMasks;
    m_flags.bindInputAssemblyState   = bindInputAssemblyState;
    m_flags.customSampleLocations    = customSampleLocations;
    m_flags.force1x1ShaderRate       = force1x1ShaderRate;
    m_flags.isPointSizeUsed          = isPointSizeUsed;
    CreateStaticState();

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

    if (ContainsStaticState(DynamicStatesInternal::Viewport))
    {
        pStaticTokens->viewport = pCache->CreateViewport(m_info.viewportParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::Scissor))
    {
        pStaticTokens->scissorRect = pCache->CreateScissorRect(m_info.scissorRectParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::LineStippleExt))
    {
        pStaticTokens->lineStippleState = pCache->CreateLineStipple(m_info.lineStippleParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::FragmentShadingRateStateKhr))
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
    CmdBuffer*                             pCmdBuffer,
    const Pal::DynamicGraphicsShaderInfos& graphicsShaderInfos
    ) const
{
    AllGpuRenderState* pRenderState = pCmdBuffer->RenderState();

    // Get this pipeline's static tokens
    const auto& newTokens = m_info.staticTokens;

    // Get the old static tokens.  Copy these by value because in MGPU cases we update the new token state in a loop.
    const auto oldTokens = pRenderState->staticTokens;

    // Program static pipeline state.

    // This code will attempt to skip programming state state based on redundant value checks.  These checks are often
    // represented as token compares, where the tokens are two perfect hashes of previously compiled pipelines' static
    // parameter values.
    // If VIEWPORT is static, VIEWPORT_COUNT must be static as well
    if (ContainsStaticState(DynamicStatesInternal::Viewport))
    {
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.viewports, newTokens.viewport))
        {
            pCmdBuffer->SetAllViewports(m_info.viewportParams, newTokens.viewport);
        }
    }
    else if (ContainsStaticState(DynamicStatesInternal::ViewportCount))
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
        }
        while (deviceGroup.IterateNext());
    }

    // We can just check DefaultDeviceIndex as the value can't vary between GPUs.
    if (pCmdBuffer->PerGpuState(DefaultDeviceIndex)->viewport.depthRange != m_info.viewportParams.depthRange)
    {
        utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
        do
        {
            pCmdBuffer->PerGpuState(deviceGroup.Index())->viewport.depthRange = m_info.viewportParams.depthRange;
        }
        while (deviceGroup.IterateNext());

        pRenderState->dirtyGraphics.viewport = 1;
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

        if (ContainsStaticState(DynamicStatesInternal::DepthTestEnableExt) &&
            (pDepthStencilCreateInfo->depthEnable != m_info.depthStencilCreateInfo.depthEnable))
        {
            pDepthStencilCreateInfo->depthEnable = m_info.depthStencilCreateInfo.depthEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthWriteEnableExt) &&
            (pDepthStencilCreateInfo->depthWriteEnable != m_info.depthStencilCreateInfo.depthWriteEnable))
        {
            pDepthStencilCreateInfo->depthWriteEnable = m_info.depthStencilCreateInfo.depthWriteEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthCompareOpExt) &&
            (pDepthStencilCreateInfo->depthFunc != m_info.depthStencilCreateInfo.depthFunc))
        {
            pDepthStencilCreateInfo->depthFunc = m_info.depthStencilCreateInfo.depthFunc;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthBoundsTestEnableExt) &&
            (pDepthStencilCreateInfo->depthBoundsEnable != m_info.depthStencilCreateInfo.depthBoundsEnable))
        {
            pDepthStencilCreateInfo->depthBoundsEnable = m_info.depthStencilCreateInfo.depthBoundsEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilTestEnableExt) &&
            (pDepthStencilCreateInfo->stencilEnable != m_info.depthStencilCreateInfo.stencilEnable))
        {
            pDepthStencilCreateInfo->stencilEnable = m_info.depthStencilCreateInfo.stencilEnable;

            pRenderState->dirtyGraphics.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilOpExt) &&
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

    if (m_flags.bindTriangleRasterState == false)
    {
        // Update the static states to renderState
        pRenderState->triangleRasterState.frontFillMode   = m_info.triangleRasterState.frontFillMode;
        pRenderState->triangleRasterState.backFillMode    = m_info.triangleRasterState.backFillMode;
        pRenderState->triangleRasterState.provokingVertex = m_info.triangleRasterState.provokingVertex;

        if (ContainsStaticState(DynamicStatesInternal::FrontFaceExt))
        {
            pRenderState->triangleRasterState.frontFace = m_info.triangleRasterState.frontFace;
        }

        if (ContainsStaticState(DynamicStatesInternal::CullModeExt))
        {
            pRenderState->triangleRasterState.cullMode = m_info.triangleRasterState.cullMode;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBiasEnableExt))
        {
            pRenderState->triangleRasterState.flags.frontDepthBiasEnable =
                m_info.triangleRasterState.flags.frontDepthBiasEnable;
            pRenderState->triangleRasterState.flags.backDepthBiasEnable =
                m_info.triangleRasterState.flags.backDepthBiasEnable;
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
        if (ContainsStaticState(DynamicStatesInternal::PrimitiveRestartEnableExt))
        {
            pRenderState->inputAssemblyState.primitiveRestartEnable = m_info.inputAssemblyState.primitiveRestartEnable;
        }

        pRenderState->inputAssemblyState.primitiveRestartIndex = m_info.inputAssemblyState.primitiveRestartIndex;
        pRenderState->inputAssemblyState.patchControlPoints    = m_info.inputAssemblyState.patchControlPoints;

        if (ContainsStaticState(DynamicStatesInternal::PrimitiveTopologyExt))
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

    utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        if (pRenderState->pGraphicsPipeline != nullptr)
        {
            bool palPipelineBound = false;
            if ((oldHash != newHash)
                )
            {
                Pal::PipelineBindParams params = {};

                params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
                params.pPipeline         = useOptimizedPipeline ? m_pOptimizedPipeline[deviceIdx] : m_pPalPipeline[deviceIdx];
                params.graphics          = graphicsShaderInfos;
                params.apiPsoHash        = m_apiHash;

                pPalCmdBuf->CmdBindPipeline(params);
                palPipelineBound         = true;
            }

            if ((palPipelineBound == false) &&
                ContainsStaticState(DynamicStatesInternal::ColorWriteEnableExt) &&
                pRenderState->lastColorWriteEnableDynamic)
            {
                // Color write enable requires an explicit write to CB_TARGET_MASK in cases where the Pal pipeline bind
                // is skipped due to matching Pal pipeline hash values and CB_TARGET_MASK has been altered by the
                // previous pipeline via color write enable.  Passing in a count of 0 resets the mask.
                pRenderState->colorWriteMaskParams.count = 0;
                pRenderState->dirtyGraphics.colorWriteEnable     = 1;
            }

            if ((palPipelineBound == false) && ContainsStaticState(DynamicStatesInternal::RasterizerDiscardEnableExt))
            {
                // Need to update static rasterizerDiscardEnable setting if the Pal pipeline bind is skipped
                pRenderState->rasterizerDiscardEnable       = m_info.rasterizerDiscardEnable;
                pRenderState->dirtyGraphics.rasterizerDiscardEnable = 1;
            }
            else if (ContainsDynamicState(DynamicStatesInternal::RasterizerDiscardEnableExt))
            {
                // Binding a pipeline overwrites the dynamic rasterizerDiscardEnable setting so need to force validation
                pRenderState->dirtyGraphics.rasterizerDiscardEnable = 1;
            }
        }
        else
        {
            Pal::PipelineBindParams params = {};

            params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
            params.pPipeline         = useOptimizedPipeline ? m_pOptimizedPipeline[deviceIdx] : m_pPalPipeline[deviceIdx];
            params.graphics          = graphicsShaderInfos;
            params.apiPsoHash        = m_apiHash;

            pPalCmdBuf->CmdBindPipeline(params);
        }

        // Bind state objects that are always static; these are redundancy checked by the pointer in the command buffer.
        if (m_flags.bindDepthStencilObject)
        {
            pCmdBuffer->PalCmdBindDepthStencilState(pPalCmdBuf, deviceIdx, m_pPalDepthStencil[deviceIdx]);

            pRenderState->dirtyGraphics.depthStencil = 0;
        }

        pCmdBuffer->PalCmdBindColorBlendState(pPalCmdBuf, deviceIdx, m_pPalColorBlend[deviceIdx]);
        pCmdBuffer->PalCmdBindMsaaState(pPalCmdBuf, deviceIdx, m_pPalMsaa[deviceIdx]);

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

        if (ContainsStaticState(DynamicStatesInternal::LineStippleExt) &&
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

        if (ContainsStaticState(DynamicStatesInternal::SampleLocationsExt) &&
            (memcmp(&pRenderState->samplePattern, &m_info.samplePattern, sizeof(SamplePattern)) != 0))
        {
            pCmdBuffer->PalCmdSetMsaaQuadSamplePattern(
                m_info.samplePattern.sampleCount, m_info.samplePattern.locations);
            pRenderState->samplePattern = m_info.samplePattern;
            pRenderState->dirtyGraphics.samplePattern = 0;
        }

        if (ContainsStaticState(DynamicStatesInternal::ColorWriteEnableExt))
        {
            if (pRenderState->lastColorWriteEnableDynamic)
            {
                 pRenderState->lastColorWriteEnableDynamic = false;
            }
            else
            {
                pRenderState->dirtyGraphics.colorWriteEnable = 0;
            }
        }

        // Only set the Fragment Shading Rate if the dynamic state is not set.
        if (ContainsStaticState(DynamicStatesInternal::FragmentShadingRateStateKhr) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.fragmentShadingRate, newTokens.fragmentShadingRate))
        {
            pPalCmdBuf->CmdSetPerDrawVrsRate(m_info.vrsRateParams);

            pRenderState->staticTokens.fragmentShadingRate = newTokens.fragmentShadingRate;
            pRenderState->dirtyGraphics.vrs = 0;
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
