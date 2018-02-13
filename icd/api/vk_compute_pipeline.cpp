/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_cmdbuffer.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_conv.h"
#include "include/vk_shader.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_memory.h"

#include "palPipeline.h"
#include "palShader.h"
#include "llpc.h"

namespace vk
{

// =====================================================================================================================
// Converts Vulkan compute pipeline parameters to an internal structure
void ComputePipeline::ConvertComputePipelineInfo(
    Device*                            pDevice,
    const VkComputePipelineCreateInfo* pIn,
    CreateInfo*                        pOutInfo)
{
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);

    if (pIn->layout != VK_NULL_HANDLE)
    {
        pOutInfo->pLayout = PipelineLayout::ObjectFromHandle(pIn->layout);
    }

    pOutInfo->flags  = pIn->flags;
    pOutInfo->pStage = &pIn->stage;

}

// =====================================================================================================================
// Creates a compute pipeline binary for each PAL device
VkResult ComputePipeline::CreateComputePipelineBinaries(
    Device*        pDevice,
    PipelineCache* pPipelineCache,
    CreateInfo*    pCreateInfo,
    size_t         pipelineBinarySizes[MaxPalDevices],
    void*          pPipelineBinaries[MaxPalDevices])
{
    VkResult               result   = VK_SUCCESS;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    const ShaderModule*    pShader  = ShaderModule::ObjectFromHandle(pCreateInfo->pStage->module);

    // Allocate space to create the LLPC/SCPC pipeline resource mappings
    void* pMappingBuffer = nullptr;

    if (pCreateInfo->pLayout != nullptr)
    {
        size_t tempBufferSize = pCreateInfo->pLayout->GetPipelineInfo()->tempBufferSize;

        // Allocate the temp buffer
        if (tempBufferSize > 0)
        {
            pMappingBuffer = pDevice->VkInstance()->AllocMem(
                tempBufferSize,
                VK_DEFAULT_MEM_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

            if (pMappingBuffer == nullptr)
            {
                result = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }

    // Build the LLPC pipeline
    Llpc::ComputePipelineBuildInfo pipelineBuildInfo   = {};
    Llpc::ComputePipelineBuildOut  pipelineOut         = {};
    void*                          pLlpcPipelineBuffer = nullptr;

    if ((result == VK_SUCCESS)
        )
    {
        // Fill pipeline create info for LLPC
        pipelineBuildInfo.pInstance      = pDevice->VkPhysicalDevice()->VkInstance();
        pipelineBuildInfo.pfnOutputAlloc = AllocateShaderOutput;
        pipelineBuildInfo.pUserData      = &pLlpcPipelineBuffer;
        auto pShaderInfo = &pipelineBuildInfo.cs;

        pShaderInfo->pModuleData         = pShader->GetShaderData(true);
        pShaderInfo->pSpecializatonInfo  = pCreateInfo->pStage->pSpecializationInfo;
        pShaderInfo->pEntryTarget        = pCreateInfo->pStage->pName;

        // Build the resource mapping description for LLPC.  This data contains things about how shader
        // inputs like descriptor set bindings interact with this pipeline in a form that LLPC can
        // understand.
        if (pCreateInfo->pLayout != nullptr)
        {
            result = pCreateInfo->pLayout->BuildLlpcPipelineMapping(
                ShaderStageCompute,
                pMappingBuffer,
                nullptr,
                pShaderInfo,
                nullptr);
        }
    }

    uint64_t pipeHash = 0;

    bool enableLlpc = false;
    enableLlpc = true;

    if (result == VK_SUCCESS)
    {
        if (enableLlpc)
        {
            if ((pPipelineCache != nullptr) && (pPipelineCache->GetPipelineCacheType() == PipelineCacheTypeLlpc))
            {
                pipelineBuildInfo.pShaderCache = pPipelineCache->GetShaderCache(DefaultDeviceIndex).pLlpcShaderCache;
            }

            auto llpcResult = pDevice->GetLlpcCompiler()->BuildComputePipeline(&pipelineBuildInfo, &pipelineOut);
            if (llpcResult != Llpc::Result::Success)
            {
                // There shouldn't be anything to free for the failure case
                VK_ASSERT(pLlpcPipelineBuffer == nullptr);

                {
                    result = VK_ERROR_INITIALIZATION_FAILED;
                }
            }
        }
        else
        if (settings.enablePipelineDump)
        {
            // LLPC isn't enabled but pipeline dump is required, call LLPC dump interface explicitly
            void* pHandle = Llpc::IPipelineDumper::BeginPipelineDump(settings.pipelineDumpDir, &pipelineBuildInfo, nullptr);
            Llpc::IPipelineDumper::EndPipelineDump(pHandle);
        }
    }

    // Update PAL pipeline create info with LLPC output
    if (enableLlpc)
    {
        if (result == VK_SUCCESS)
        {

            // Make sure that this is the same pointer we will free once the PAL pipeline is created
            VK_ASSERT(pLlpcPipelineBuffer == pipelineOut.pipelineBin.pCode);

            pPipelineBinaries[DefaultDeviceIndex]   = pLlpcPipelineBuffer;
            pipelineBinarySizes[DefaultDeviceIndex] = pipelineOut.pipelineBin.codeSize;
        }
    }
    else
    {
        result = VK_SUCCESS;
    }

    // Free the memory for the LLPC/SCPC pipeline resource mappings
    if (pMappingBuffer != nullptr)
    {
        pDevice->VkInstance()->FreeMem(pMappingBuffer);
    }

    return result;
}

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* const                        pDevice,
    Pal::IPipeline**                     pPalPipeline,
    const PipelineLayout*                pPipelineLayout,
    PipelineBinaryInfo*                  pPipelineBinary,
    const ImmedInfo&                     immedInfo)
    :
    Pipeline(pDevice, pPalPipeline, pPipelineLayout, pPipelineBinary),
    m_info(immedInfo)
{
    CreateStaticState();
}

// =====================================================================================================================
// Creates instances of static pipeline state.  Much of this information can be cached at the device-level to help speed
// up pipeline-bind operations.
void ComputePipeline::CreateStaticState()
{
}

// =====================================================================================================================
// Destroys static pipeline state.
void ComputePipeline::DestroyStaticState(
    const VkAllocationCallbacks* pAllocator)
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();

    pCache->DestroyComputeWaveLimits(m_info.computeWaveLimitParams,
        m_info.staticTokens.waveLimits);
}

// =====================================================================================================================
VkResult ComputePipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    DestroyStaticState(pAllocator);

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
// Create a compute pipeline object.
VkResult ComputePipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkComputePipelineCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    // Setup PAL create info from Vulkan inputs
    CreateInfo  createInfo                         = {};
    size_t      pipelineBinarySizes[MaxPalDevices] = {};
    void*       pPipelineBinaries[MaxPalDevices]   = {};

    ConvertComputePipelineInfo(pDevice, pCreateInfo, &createInfo);

    VkResult result = CreateComputePipelineBinaries(
        pDevice,
        pPipelineCache,
        &createInfo,
        pipelineBinarySizes,
        pPipelineBinaries);

    if (result != VK_SUCCESS)
    {
        return result;
    }

    size_t pipelineSize = 0;
    void*  pSystemMem   = nullptr;

    if (result == VK_SUCCESS)
    {
        // Get the pipeline and shader size from PAL and allocate memory.
        pipelineSize = pDevice->PalDevice()->GetComputePipelineSize(createInfo.pipeline, nullptr);

        pSystemMem = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(ComputePipeline) + (pipelineSize * pDevice->NumPalDevices()),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Create the PAL pipeline object.
    Pal::IPipeline* pPalPipeline[MaxPalDevices] = {};

    if (result == VK_SUCCESS)
    {
        Pal::Result palResult = Pal::Result::Success;
        void*       pPalMem   = Util::VoidPtrInc(pSystemMem, sizeof(ComputePipeline));

        for (uint32_t deviceIdx = 0;
            ((deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
            deviceIdx++)
        {
            VK_ASSERT(pipelineSize == pDevice->PalDevice(deviceIdx)->GetComputePipelineSize(createInfo.pipeline, nullptr));

            // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
            // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
            if (pPipelineBinaries[deviceIdx] != nullptr)
            {
                createInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[deviceIdx];
                createInfo.pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
            }

            palResult = pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                createInfo.pipeline,
                Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                &pPalPipeline[deviceIdx]);
        }

        result = PalToVkResult(palResult);
    }

    // Retain a copy of the pipeline binary if an extension that can query it is enabled
    PipelineBinaryInfo* pBinary = nullptr;

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) && (result == VK_SUCCESS))
    {
        pBinary = PipelineBinaryInfo::Create(pipelineBinarySizes[DefaultDeviceIndex],
                                             pPipelineBinaries[DefaultDeviceIndex],
                                             pAllocator);
    }

    if (result == VK_SUCCESS)
    {
        // On success, wrap it up in a Vulkan object and return.
        VK_PLACEMENT_NEW(pSystemMem) ComputePipeline(pDevice,
                                                     pPalPipeline,
                                                     createInfo.pLayout,
                                                     pBinary,
                                                     createInfo.immedInfo);

        *pPipeline = ComputePipeline::HandleFromVoidPointer(pSystemMem);
    }
    else
    {
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            // Internal memory allocation failed, free PAL event object if it gets created
            if (pPalPipeline[deviceIdx] != nullptr)
            {
                pPalPipeline[deviceIdx]->Destroy();
            }
        }
    }

    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            {
                pDevice->VkInstance()->FreeMem(pPipelineBinaries[deviceIdx]);
            }
        }
    }

    // Something went wrong with creating the PAL object. Free memory and return error.
    if (result != VK_SUCCESS)
    {
        // Free system memory for pipeline object
        pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);

        if (pBinary != nullptr)
        {
            pBinary->Destroy(pAllocator);
        }
    }

    return result;
}

// =====================================================================================================================
void ComputePipeline::BindToCmdBuffer(CmdBuffer* pCmdBuffer) const
{
    BindToCmdBuffer(pCmdBuffer, m_info.computeWaveLimitParams);
}

// =====================================================================================================================
void ComputePipeline::BindToCmdBuffer(
    CmdBuffer*                           pCmdBuffer,
    const Pal::DynamicComputeShaderInfo& computeShaderInfo) const
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint = Pal::PipelineBindPoint::Compute;
    params.cs = computeShaderInfo;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        params.pPipeline = m_pPalPipeline[deviceIdx];

        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

// =====================================================================================================================
void ComputePipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numGroupedCmdBuffers = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint       = Pal::PipelineBindPoint::Compute;

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

} // namespace vk
