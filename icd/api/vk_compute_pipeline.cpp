/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palMetroHash.h"

namespace vk
{

// =====================================================================================================================
// Generates the API PSO hash using the contents of the VkComputePipelineCreateInfo struct
// Pipeline compilation affected by:
//     - pCreateInfo->pStage
//     - pCreateInfo->layout
uint64_t ComputePipeline::BuildApiHash(
    const VkComputePipelineCreateInfo* pCreateInfo,
    Util::MetroHash::Hash*             pBaseHash)
{
    Util::MetroHash128 baseHasher;
    Util::MetroHash128 apiHasher;

    baseHasher.Update(pCreateInfo->flags);

    GenerateHashFromShaderStageCreateInfo(&baseHasher, pCreateInfo->stage);

    baseHasher.Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());

    if ((pCreateInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) && (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(Pipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);

    baseHasher.Finalize(reinterpret_cast<uint8_t* const>(pBaseHash));

    uint64_t              apiHash;
    Util::MetroHash::Hash apiHashFull;
    apiHasher.Update(*pBaseHash);
    apiHasher.Finalize(reinterpret_cast<uint8_t* const>(&apiHashFull));
    apiHash = Util::MetroHash::Compact64(&apiHashFull);

    return apiHash;
}

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

    pOutInfo->staticStateMask = 0;

}

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* const                        pDevice,
    Pal::IPipeline**                     pPalPipeline,
    const PipelineLayout*                pPipelineLayout,
    PipelineBinaryInfo*                  pPipelineBinary,
    const ImmedInfo&                     immedInfo,
    uint32_t                             staticStateMask,
    uint64_t                             apiHash)
    :
    Pipeline(pDevice),
    m_info(immedInfo)
{
    Pipeline::Init(pPalPipeline, pPipelineLayout, pPipelineBinary, staticStateMask, apiHash);

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
    int64_t startTime = Util::GetPerfCpuTime();

    // Setup PAL create info from Vulkan inputs
    size_t                    pipelineBinarySizes[MaxPalDevices] = {};
    const void*               pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash     cacheId[MaxPalDevices]             = {};
    PipelineCompiler*         pDefaultCompiler                   = pDevice->GetCompiler(DefaultDeviceIndex);
    ComputePipelineCreateInfo binaryCreateInfo                   = {};
    uint64_t                  apiPsoHash                         = BuildApiHash(pCreateInfo, &binaryCreateInfo.basePipelineHash);

    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo = nullptr;
    VkResult result = pDefaultCompiler->ConvertComputePipelineInfo(
        pDevice, pCreateInfo, &binaryCreateInfo, &pPipelineCreationFeadbackCreateInfo);

    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&binaryCreateInfo.pipelineInfo);
    for (uint32_t deviceIdx = 0; (result == VK_SUCCESS) && (deviceIdx < pDevice->NumPalDevices()); deviceIdx++)
    {
        result = pDevice->GetCompiler(deviceIdx)->CreateComputePipelineBinary(
            pDevice,
            deviceIdx,
            pPipelineCache,
            &binaryCreateInfo,
            &pipelineBinarySizes[deviceIdx],
            &pPipelineBinaries[deviceIdx],
            &cacheId[deviceIdx]);
    }

    if (result != VK_SUCCESS)
    {
        return result;
    }

    CreateInfo localPipelineInfo = {};

    if (result == VK_SUCCESS)
    {
        ConvertComputePipelineInfo(pDevice, pCreateInfo, &localPipelineInfo);

        // Override pipeline creation parameters based on pipeline profile
        pDevice->GetShaderOptimizer()->OverrideComputePipelineCreateInfo(
            binaryCreateInfo.pipelineProfileKey,
            nullptr);
    }

    // Get the pipeline and shader size from PAL and allocate memory.
    size_t pipelineSize = 0;
    void*  pSystemMem   = nullptr;

    Pal::Result palResult = Pal::Result::Success;

    if (result == VK_SUCCESS)
    {
        localPipelineInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[DefaultDeviceIndex];
        localPipelineInfo.pipeline.pPipelineBinary    = pPipelineBinaries[DefaultDeviceIndex];

        pipelineSize =
            pDevice->PalDevice(DefaultDeviceIndex)->GetComputePipelineSize(localPipelineInfo.pipeline, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        pSystemMem = pDevice->AllocApiObject(
            pAllocator,
            sizeof(ComputePipeline) + (pipelineSize * pDevice->NumPalDevices()));

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Create the PAL pipeline object.
    Pal::IPipeline* pPalPipeline[MaxPalDevices] = {};

    if (result == VK_SUCCESS)
    {
        void* pPalMem = Util::VoidPtrInc(pSystemMem, sizeof(ComputePipeline));

        for (uint32_t deviceIdx = 0;
            ((deviceIdx < pDevice->NumPalDevices()) && (palResult == Pal::Result::Success));
            deviceIdx++)
        {
            VK_ASSERT(pipelineSize ==
                pDevice->PalDevice(deviceIdx)->GetComputePipelineSize(localPipelineInfo.pipeline, nullptr));

            // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
            // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
            if (pPipelineBinaries[deviceIdx] != nullptr)
            {
                localPipelineInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[deviceIdx];
                localPipelineInfo.pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
            }

            palResult = pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                localPipelineInfo.pipeline,
                Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
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
                    &cacheId[deviceIdx],
                    &localPipelineInfo.pipeline.pipelineBinarySize,
                    &localPipelineInfo.pipeline.pPipelineBinary,
                    pPipelineCache);

                if (palResult == Util::Result::Success)
                {
                    pPalPipeline[deviceIdx]->Destroy();

                    palResult = pDevice->PalDevice(deviceIdx)->CreateComputePipeline(
                        localPipelineInfo.pipeline,
                        Util::VoidPtrInc(pPalMem, deviceIdx * pipelineSize),
                        &pPalPipeline[deviceIdx]);
                }
                else if (palResult == Util::Result::NotFound)
                {
                    // If a replacement was not found, proceed with the original
                    palResult = Util::Result::Success;
                }
            }
#endif
        }

        result = PalToVkResult(palResult);
    }

    // Retain a copy of the pipeline binary if an extension that can query it is enabled
    PipelineBinaryInfo* pBinary = nullptr;

    if ((pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) ||
        (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_PIPELINE_EXECUTABLE_PROPERTIES)  &&
        ((pCreateInfo->flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) != 0))) &&
        (result == VK_SUCCESS))
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
                                                     localPipelineInfo.pLayout,
                                                     pBinary,
                                                     localPipelineInfo.immedInfo,
                                                     localPipelineInfo.staticStateMask,
                                                     apiPsoHash);

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
            pDevice->GetCompiler(deviceIdx)->FreeComputePipelineBinary(
                &binaryCreateInfo, pPipelineBinaries[deviceIdx], pipelineBinarySizes[deviceIdx]);
        }
    }

    pDefaultCompiler->FreeComputePipelineCreateInfo(&binaryCreateInfo);

    // Something went wrong with creating the PAL object. Free memory and return error.
    if (result != VK_SUCCESS)
    {
        // Free system memory for pipeline object
        pDevice->FreeApiObject(pAllocator, pSystemMem);

        if (pBinary != nullptr)
        {
            pBinary->Destroy(pAllocator);
        }
    }

    if (result == VK_SUCCESS)
    {
        uint64_t duration = Util::GetPerfCpuTime() - startTime;
        binaryCreateInfo.pipelineFeedback.feedbackValid = true;
        binaryCreateInfo.pipelineFeedback.duration = duration;
        pDefaultCompiler->SetPipelineCreationFeedbackInfo(
                pPipelineCreationFeadbackCreateInfo,
                &binaryCreateInfo.pipelineFeedback);

        const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
        // The hash is same as pipline dump file name, we can easily analyze further.
        AmdvlkLog(settings.logTagIdMask, PipelineCompileTime, "0x%016llX-%llu", pipelineHash, duration);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
    params.apiPsoHash = m_apiHash;
#endif

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
    params.apiPsoHash = Pal::InternalApiPsoHash;
#endif

    for (uint32_t deviceIdx = 0; deviceIdx < numGroupedCmdBuffers; deviceIdx++)
    {
        pCmdBuffer->PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
    }
}

} // namespace vk
