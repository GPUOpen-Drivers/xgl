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
/**
 ***********************************************************************************************************************
 * @file  vk_gpa_session.cpp
 * @brief Contains implementation of VkGpaSession object (VK_AMD_gpa)
 ***********************************************************************************************************************
 */

#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_gpa_session.h"

#include "palLib.h"

namespace vk
{

// =====================================================================================================================
VkResult GpaSession::Create(
    Device*                          pDevice,
    const VkGpaSessionCreateInfoAMD* pCreateInfo,
    const VkAllocationCallbacks*     pAllocator,
    VkGpaSessionAMD*                 pGpaSession)
{
    VkResult result = VK_SUCCESS;

    pAllocator = (pAllocator != nullptr) ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    void* pStorage = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        sizeof(GpaSession),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pStorage != nullptr)
    {
        GpaSession* pSession;

        if (pCreateInfo->secondaryCopySource == VK_NULL_HANDLE)
        {
            pSession = VK_PLACEMENT_NEW(pStorage) GpaSession(pDevice);
        }
        else
        {
            const GpaSession& parent = *GpaSession::ObjectFromHandle(pCreateInfo->secondaryCopySource);

            pSession = VK_PLACEMENT_NEW(pStorage) GpaSession(parent);
        }

        result = pSession->Init();

        if (result == VK_SUCCESS)
        {
            *pGpaSession = GpaSession::HandleFromObject(pSession);
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
GpaSession::GpaSession(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_session(
        pDevice->VkInstance()->PalPlatform(),
        pDevice->PalDevice(DefaultDeviceIndex),
        0,
        0,
        GpuUtil::ApiType::Vulkan,
        0)
{

}

// =====================================================================================================================
GpaSession::GpaSession(
    const GpaSession& session)
    :
    m_pDevice(session.m_pDevice),
    m_session(session.m_session)
{

}

// =====================================================================================================================
VkResult GpaSession::Init()
{
    Pal::Result result = m_session.Init();

    return PalToVkResult(result);
}

// =====================================================================================================================
void GpaSession::Destroy(
    const VkAllocationCallbacks* pAllocator)
{
    pAllocator = (pAllocator != nullptr) ? pAllocator : m_pDevice->VkInstance()->GetAllocCallbacks();

    Util::Destructor(this);

    pAllocator->pfnFree(pAllocator->pUserData, this);
}

// =====================================================================================================================
VkResult GpaSession::GetResults(
    uint32_t sampleId,
    size_t*  pSizeInBytes,
    void*    pData)
{
    if (sampleId != GpuUtil::InvalidSampleId)
    {
        Pal::Result result = m_session.GetResults(sampleId, pSizeInBytes, pData);

        if (result == Pal::Result::ErrorUnavailable)
        {
            return VK_NOT_READY;
        }
        else if (result == Pal::Result::ErrorInvalidMemorySize)
        {
            return VK_INCOMPLETE;
        }
        else
        {
            return PalToVkResult(result);
        }
    }
    else
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
}

// =====================================================================================================================
VkResult GpaSession::Reset()
{
    Pal::Result result = m_session.Reset();

    return PalToVkResult(result);
}

// =====================================================================================================================
VkResult GpaSession::CmdBegin(CmdBuffer* pCmdBuf)
{
    GpuUtil::GpaSessionBeginInfo beginInfo = {};

    Pal::Result palResult = m_session.Begin(beginInfo);

    VkResult result = PalToVkResult(palResult);

    return result;
}

// =====================================================================================================================
VkResult GpaSession::CmdEnd(CmdBuffer* pCmdBuf)
{
    Pal::Result palResult = m_session.End(pCmdBuf->PalCmdBuffer(DefaultDeviceIndex));

    VkResult result = PalToVkResult(palResult);

    return result;
}

// =====================================================================================================================
static VkResult ConvertPerfCounterId(
    const VkGpaPerfCounterAMD& perfCounter,
    GpuUtil::PerfCounterId*    pId)
{
    VkResult result = VK_SUCCESS;

    pId->block    = VkToPalGpuBlock(perfCounter.blockType);
    pId->instance = perfCounter.blockInstance;
    pId->eventId  = perfCounter.eventID;

    if (pId->block == Pal::GpuBlock::Count)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    return result;
}

// =====================================================================================================================
VkResult GpaSession::CmdBeginSample(
    CmdBuffer*                     pCmdbuf,
    const VkGpaSampleBeginInfoAMD* pGpaSampleBeginInfo,
    uint32_t*                      pSampleID)
{
    VkResult result = VK_SUCCESS;

    const PhysicalDeviceGpaProperties& gpaProps = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetGpaProperties();

    GpuUtil::GpaSampleConfig sampleConfig = {};

    if (pGpaSampleBeginInfo->sampleType == VK_GPA_SAMPLE_TYPE_CUMULATIVE_AMD)
    {
        sampleConfig.type = GpuUtil::GpaSampleType::Cumulative;
    }
    else if (pGpaSampleBeginInfo->sampleType == VK_GPA_SAMPLE_TYPE_TRACE_AMD)
    {
        sampleConfig.type = GpuUtil::GpaSampleType::Trace;
    }
    else if (pGpaSampleBeginInfo->sampleType == VK_GPA_SAMPLE_TYPE_TIMING_AMD)
    {
        sampleConfig.type = GpuUtil::GpaSampleType::Timing;
    }
    else
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    sampleConfig.flags.sampleInternalOperations      = pGpaSampleBeginInfo->sampleInternalOperations;
    sampleConfig.flags.cacheFlushOnCounterCollection = pGpaSampleBeginInfo->cacheFlushOnCounterCollection;
    sampleConfig.flags.sqShaderMask                  = pGpaSampleBeginInfo->sqShaderMaskEnable;

#if VKI_BUILD_GFX11
    sampleConfig.flags.sqWgpShaderMask               = pGpaSampleBeginInfo->sqShaderMaskEnable;
#endif
    sampleConfig.sqShaderMask = static_cast<Pal::PerfExperimentShaderFlags>(
        VkToPalPerfExperimentShaderFlags(pGpaSampleBeginInfo->sqShaderMask));

#if VKI_BUILD_GFX11
    sampleConfig.sqWgpShaderMask = static_cast<Pal::PerfExperimentShaderFlags>(
        VkToPalPerfExperimentShaderFlags(pGpaSampleBeginInfo->sqShaderMask));
#endif

    VirtualStackFrame virtStackFrame(pCmdbuf->GetStackAllocator());

    sampleConfig.perfCounters.numCounters = pGpaSampleBeginInfo->perfCounterCount;

    if (sampleConfig.perfCounters.numCounters > 0)
    {
        auto* pPalCounters = virtStackFrame.AllocArray<GpuUtil::PerfCounterId>(sampleConfig.perfCounters.numCounters);

        sampleConfig.perfCounters.pIds = pPalCounters;

        if (pPalCounters != nullptr)
        {
            for (uint32_t i = 0; (i < sampleConfig.perfCounters.numCounters) && (result == VK_SUCCESS); ++i)
            {
                result = ConvertPerfCounterId(pGpaSampleBeginInfo->pPerfCounters[i], &pPalCounters[i]);

                VK_ASSERT((result != VK_SUCCESS) ||
                          (gpaProps.palProps.blocks[static_cast<size_t>(pPalCounters[i].block)].available));
            }
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    sampleConfig.perfCounters.spmTraceSampleInterval = pGpaSampleBeginInfo->streamingPerfTraceSampleInterval;
    sampleConfig.perfCounters.gpuMemoryLimit         = pGpaSampleBeginInfo->perfCounterDeviceMemoryLimit;

    sampleConfig.sqtt.flags.enable                   = pGpaSampleBeginInfo->sqThreadTraceEnable;
    sampleConfig.sqtt.flags.supressInstructionTokens = pGpaSampleBeginInfo->sqThreadTraceSuppressInstructionTokens;
    sampleConfig.sqtt.flags.stallMode                = Pal::GpuProfilerStallMode::GpuProfilerStallAlways;
    sampleConfig.sqtt.seMask                         = UINT32_MAX;
    sampleConfig.sqtt.gpuMemoryLimit                 = pGpaSampleBeginInfo->sqThreadTraceDeviceMemoryLimit;

    sampleConfig.timing.preSample  = VkToPalSrcPipePointForTimestampWrite(pGpaSampleBeginInfo->timingPreSample);
    sampleConfig.timing.postSample = VkToPalSrcPipePointForTimestampWrite(pGpaSampleBeginInfo->timingPostSample);

    if (result == VK_SUCCESS)
    {
        result = PalToVkResult(
            m_session.BeginSample(pCmdbuf->PalCmdBuffer(DefaultDeviceIndex), sampleConfig, pSampleID));
    }

    if (sampleConfig.perfCounters.pIds != nullptr)
    {
        virtStackFrame.FreeArray(sampleConfig.perfCounters.pIds);
    }

    return result;
}

// =====================================================================================================================
void GpaSession::CmdEndSample(
    CmdBuffer* pCmdbuf,
    uint32_t   sampleID)
{
    if (sampleID != GpuUtil::InvalidSampleId)
    {
        m_session.EndSample(pCmdbuf->PalCmdBuffer(DefaultDeviceIndex), sampleID);
    }
}

// =====================================================================================================================
void GpaSession::CmdCopyResults(
    CmdBuffer* pCmdBuf)
{
    m_session.CopyResults(pCmdBuf->PalCmdBuffer(DefaultDeviceIndex));
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGpaSessionAMD(
    VkDevice                                    device,
    const VkGpaSessionCreateInfoAMD*            pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkGpaSessionAMD*                            pGpaSession)
{
    VkResult result = GpaSession::Create(ApiDevice::ObjectFromHandle(device), pCreateInfo, pAllocator, pGpaSession);

    return result;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyGpaSessionAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession,
    const VkAllocationCallbacks*                pAllocator)
{
    GpaSession::ObjectFromHandle(gpaSession)->Destroy(pAllocator);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCmdBeginGpaSessionAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->CmdBegin(ApiCmdBuffer::ObjectFromHandle(commandBuffer));

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCmdEndGpaSessionAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->CmdEnd(ApiCmdBuffer::ObjectFromHandle(commandBuffer));

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCmdBeginGpaSampleAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession,
    const VkGpaSampleBeginInfoAMD*              pGpaSampleBeginInfo,
    uint32_t*                                   pSampleID)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->CmdBeginSample(
        ApiCmdBuffer::ObjectFromHandle(commandBuffer),
        pGpaSampleBeginInfo,
        pSampleID);

    return result;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndGpaSampleAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession,
    uint32_t                                    sampleID)
{
    GpaSession::ObjectFromHandle(gpaSession)->CmdEndSample(ApiCmdBuffer::ObjectFromHandle(commandBuffer), sampleID);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetGpaSessionStatusAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->GetStatus();

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkGetGpaSessionResultsAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession,
    uint32_t                                    sampleID,
    size_t*                                     pSizeInBytes,
    void*                                       pData)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->GetResults(
        sampleID,
        pSizeInBytes,
        pData);

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkResetGpaSessionAMD(
    VkDevice                                    device,
    VkGpaSessionAMD                             gpaSession)
{
    VkResult result = GpaSession::ObjectFromHandle(gpaSession)->Reset();

    return result;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyGpaSessionResultsAMD(
    VkCommandBuffer                             commandBuffer,
    VkGpaSessionAMD                             gpaSession)
{
    GpaSession::ObjectFromHandle(gpaSession)->CmdCopyResults(ApiCmdBuffer::ObjectFromHandle(commandBuffer));
}

} // namespace entry

} // namespace vk
