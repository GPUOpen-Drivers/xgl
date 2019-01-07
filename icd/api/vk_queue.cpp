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
/**
 ***********************************************************************************************************************
 * @file  vk_queue.cpp
 * @brief Contains implementation of Vulkan query pool.
 ***********************************************************************************************************************
 */

#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_display.h"
#include "include/vk_fence.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_object.h"
#include "include/vk_queue.h"
#include "include/vk_semaphore.h"
#include "include/vk_swapchain.h"
#include "include/vk_utils.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

#include "palQueue.h"

namespace vk
{

// =====================================================================================================================
Queue::Queue(
    Device*                 pDevice,
    uint32_t                queueFamilyIndex,
    uint32_t                queueIndex,
    uint32_t                queueFlags,
    Pal::IQueue**           pPalQueues,
    VirtualStackAllocator*  pStackAllocator)
    :
    m_pDevice(pDevice),
    m_queueFamilyIndex(queueFamilyIndex),
    m_queueIndex(queueIndex),
    m_queueFlags(queueFlags),
    m_pDevModeMgr(pDevice->VkInstance()->GetDevModeMgr()),
    m_pStackAllocator(pStackAllocator)
{
    memcpy(m_pPalQueues, pPalQueues, sizeof(pPalQueues[0]) * pDevice->NumPalDevices());
    memset(&m_palFrameMetadataControl, 0, sizeof(Pal::PerSourceFrameMetadataControl));
    memset(&m_pDummyCmdBuffer, 0, sizeof(m_pDummyCmdBuffer[0]) * pDevice->NumPalDevices());

}

// =====================================================================================================================
Queue::~Queue()
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (m_pDummyCmdBuffer[deviceIdx] != nullptr)
        {
            m_pDummyCmdBuffer[deviceIdx]->Destroy();
            m_pDevice->VkInstance()->FreeMem(m_pDummyCmdBuffer[deviceIdx]);
        }
    }

    if (m_pStackAllocator != nullptr)
    {
        // Release the stack allocator
        m_pDevice->VkInstance()->StackMgr()->ReleaseAllocator(m_pStackAllocator);
    }

    if (m_pPalQueues != nullptr)
    {
        for (uint32_t i = 0; i < m_pDevice->NumPalDevices(); i++)
        {
            PalQueue(i)->Destroy();
        }
    }

}

// =====================================================================================================================
// Create a dummy command buffer for the present queue
VkResult Queue::CreateDummyCmdBuffer()
{
    Pal::Result palResult = Pal::Result::ErrorUnknown;

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        Pal::CmdBufferCreateInfo palCreateInfo = {};
        palCreateInfo.pCmdAllocator = m_pDevice->GetSharedCmdAllocator(deviceIdx);
        palCreateInfo.queueType     = m_pDevice->GetQueueFamilyPalQueueType(m_queueFamilyIndex);
        palCreateInfo.engineType    = m_pDevice->GetQueueFamilyPalEngineType(m_queueFamilyIndex);

        Pal::IDevice* const pPalDevice = m_pDevice->PalDevice(deviceIdx);
        const size_t palSize = pPalDevice->GetCmdBufferSize(palCreateInfo, &palResult);

        if (palResult == Pal::Result::Success)
        {
            void* pMemory = m_pDevice->VkInstance()->AllocMem(palSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
            if (pMemory != nullptr)
            {
                palResult = pPalDevice->CreateCmdBuffer(palCreateInfo, pMemory, &(m_pDummyCmdBuffer[deviceIdx]));

                if (palResult == Pal::Result::Success)
                {
                    Pal::CmdBufferBuildInfo buildInfo = {};
                    buildInfo.flags.optimizeExclusiveSubmit = 1;
                    palResult = m_pDummyCmdBuffer[deviceIdx]->Begin(buildInfo);
                    if (palResult == Pal::Result::Success)
                    {
                        palResult = m_pDummyCmdBuffer[deviceIdx]->End();
                    }
                }
            }
            else
            {
                palResult = Pal::Result::ErrorOutOfMemory;
            }
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Submit a dummy command buffer with associated command buffer info to KMD for FRTC/TurboSync/DVR features
VkResult Queue::NotifyFlipMetadata(
    uint32_t                     deviceIdx,
    Pal::IQueue*                 pQueue,
    const Pal::IGpuMemory*       pGpuMemory,
    FullscreenFrameMetadataFlags flags)
{
    VkResult result = VK_SUCCESS;
    if (m_pDummyCmdBuffer[deviceIdx] == nullptr)
    {
        result = CreateDummyCmdBuffer();
        VK_ASSERT(result == VK_SUCCESS);
    }

    if (m_pDummyCmdBuffer[deviceIdx] != nullptr)
    {
        if ((flags.frameBeginFlag == 1) || (flags.frameEndFlag == 1) || (flags.primaryHandle == 1))
        {
            Pal::CmdBufInfo cmdBufInfo = {};
            cmdBufInfo.isValid = 1;
            if (flags.frameBeginFlag == 1)
            {
                cmdBufInfo.frameBegin = 1;
            }
            else if (flags.frameEndFlag == 1)
            {
                cmdBufInfo.frameEnd = 1;
            }

            if (flags.primaryHandle == 1)
            {
                cmdBufInfo.pPrimaryMemory = pGpuMemory;
            }

            Pal::SubmitInfo submitInfo = {};

            submitInfo.cmdBufferCount  = 1;
            submitInfo.ppCmdBuffers    = &(m_pDummyCmdBuffer[deviceIdx]);
            submitInfo.pCmdBufInfoList = &cmdBufInfo;

            result = PalToVkResult(m_pPalQueues[deviceIdx]->Submit(submitInfo));
            VK_ASSERT(result == VK_SUCCESS);
        }
    }

    return result;
}

// =====================================================================================================================
// Submit command buffer info with frameEndFlag and primaryHandle before frame present
VkResult Queue::NotifyFlipMetadataBeforePresent(
    uint32_t                         deviceIdx,
    Pal::IQueue*                     pQueue,
    const Pal::PresentSwapChainInfo* pPresentInfo,
    const Pal::IGpuMemory*           pGpuMemory)
{
    VkResult result = VK_SUCCESS;
    return result;
}

// =====================================================================================================================
// Submit command buffer info with frameBeginFlag after frame present
VkResult Queue::NotifyFlipMetadataAfterPresent(
    uint32_t                         deviceIdx,
    Pal::IQueue*                     pQueue,
    const Pal::PresentSwapChainInfo* pPresentInfo)
{
    VkResult result = VK_SUCCESS;

    return result;
}

// =====================================================================================================================
// Submit an array of command buffers to a queue
VkResult Queue::Submit(
    uint32_t            submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence             fence)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) && pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif
    Fence* pFence = Fence::ObjectFromHandle(fence);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    VkResult result = VK_SUCCESS;

    // The fence should be only used in the last submission to PAL. The implicit ordering guarantees provided by PAL
    // make sure that the fence is only signaled when all submissions complete.
    if ((submitCount == 0) && (pFence != nullptr))
    {
        // If the submit count is zero but there is a fence, do a dummy submit just so the fence is signaled.
        Pal::SubmitInfo submitInfo = {};

        submitInfo.cmdBufferCount  = 0;
        submitInfo.ppCmdBuffers    = nullptr;
        submitInfo.pCmdBufInfoList = nullptr;
        submitInfo.gpuMemRefCount  = 0;
        submitInfo.pGpuMemoryRefs  = nullptr;

        Pal::Result palResult = Pal::Result::Success;

        pFence->SetActiveDevice(DefaultDeviceIndex);

        submitInfo.pFence = pFence->PalFence(DefaultDeviceIndex);
        palResult = PalQueue(DefaultDeviceIndex)->Submit(submitInfo);

        result = PalToVkResult(palResult);
    }
    else
    {
        for (uint32_t submitIdx = 0; (submitIdx < submitCount) && (result == VK_SUCCESS); ++submitIdx)
        {
            const VkSubmitInfo& submitInfo = pSubmits[submitIdx];
            const VkDeviceGroupSubmitInfo* pDeviceGroupInfo = nullptr;
            {
                union
                {
                    const VkStructHeader*                          pHeader;
                    const VkSubmitInfo*                            pVkSubmitInfo;
                    const VkDeviceGroupSubmitInfo*                 pVkDeviceGroupSubmitInfo;
                };

                for (pVkSubmitInfo = &submitInfo; pHeader != nullptr; pHeader = pHeader->pNext)
                {
                    switch (static_cast<int32_t>(pHeader->sType))
                    {
                    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO:
                        pDeviceGroupInfo = pVkDeviceGroupSubmitInfo;
                        break;
                    default:
                        // Skip any unknown extension structures
                        break;
                    }
                }
            }

            if ((result == VK_SUCCESS) && (submitInfo.waitSemaphoreCount > 0))
            {
                result = PalWaitSemaphores(
                    submitInfo.waitSemaphoreCount,
                    submitInfo.pWaitSemaphores,
                    nullptr,
                    (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->waitSemaphoreCount          : 0),
                    (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->pWaitSemaphoreDeviceIndices : nullptr));
            }

            // Allocate space to store the PAL command buffer handles
            const VkCommandBuffer* pCmdBuffers = submitInfo.pCommandBuffers;
            const uint32_t cmdBufferCount      = submitInfo.commandBufferCount;

            Pal::ICmdBuffer** pPalCmdBuffers = (cmdBufferCount > 0) ?
                            virtStackFrame.AllocArray<Pal::ICmdBuffer*>(submitInfo.commandBufferCount) : nullptr;

            result = ((pPalCmdBuffers != nullptr) || (cmdBufferCount == 0)) ? result : VK_ERROR_OUT_OF_HOST_MEMORY;

            bool lastBatch = (submitIdx == submitCount - 1);

            Pal::SubmitInfo palSubmitInfo = {};

            palSubmitInfo.ppCmdBuffers    = pPalCmdBuffers;
            palSubmitInfo.pCmdBufInfoList = nullptr;
            palSubmitInfo.gpuMemRefCount  = 0;
            palSubmitInfo.pGpuMemoryRefs  = nullptr;

            const uint32_t deviceCount = (pDeviceGroupInfo == nullptr) ? 1 : m_pDevice->NumPalDevices();

            for (uint32_t deviceIdx = 0; (deviceIdx < deviceCount) && (result == VK_SUCCESS); deviceIdx++)
            {
                // Get the PAL command buffer object from each Vulkan object and put it
                // in the local array before submitting to PAL.
                DispatchableCmdBuffer* const * pCommandBuffers =
                    reinterpret_cast<DispatchableCmdBuffer*const*>(submitInfo.pCommandBuffers);

                if (pDeviceGroupInfo == nullptr)
                {
                    palSubmitInfo.cmdBufferCount = cmdBufferCount;

                    for (uint32_t i = 0; i < cmdBufferCount; ++i)
                    {
                        const CmdBuffer& cmdBuf = *(*pCommandBuffers[i]);

                        pPalCmdBuffers[i] = cmdBuf.PalCmdBuffer(deviceIdx);
                    }
                }
                else
                {
                    palSubmitInfo.cmdBufferCount = 0;

                    const uint32_t deviceMask = 1 << deviceIdx;

                    for (uint32_t i = 0; i < cmdBufferCount; ++i)
                    {
                        const CmdBuffer& cmdBuf = *(*pCommandBuffers[i]);

                        if ((pDeviceGroupInfo->pCommandBufferDeviceMasks != nullptr) &&
                            (pDeviceGroupInfo->pCommandBufferDeviceMasks[i] & deviceMask) == 0)
                        {
                            continue;
                        }

                        pPalCmdBuffers[palSubmitInfo.cmdBufferCount++] = cmdBuf.PalCmdBuffer(deviceIdx);
                    }
                }

                if (lastBatch && (pFence != nullptr))
                {
                    palSubmitInfo.pFence = pFence->PalFence(deviceIdx);

                    pFence->SetActiveDevice(deviceIdx);
                }

                if ((palSubmitInfo.cmdBufferCount > 0) ||
                    (palSubmitInfo.pFence != nullptr)  ||
                    (submitInfo.waitSemaphoreCount > 0))
                {
                    Pal::Result palResult = Pal::Result::Success;

                    if (timedQueueEvents == false)
                    {
                        palResult = PalQueue(deviceIdx)->Submit(palSubmitInfo);
                    }
                    else
                    {
#if ICD_GPUOPEN_DEVMODE_BUILD
                        palResult = m_pDevModeMgr->TimedQueueSubmit(deviceIdx,
                            this,
                            cmdBufferCount,
                            submitInfo.pCommandBuffers,
                            palSubmitInfo,
                            &virtStackFrame);
#else
                        VK_NEVER_CALLED();
#endif
                    }
                    result = PalToVkResult(palResult);
                }

            }

            virtStackFrame.FreeArray(pPalCmdBuffers);

            if ((result == VK_SUCCESS) && (submitInfo.signalSemaphoreCount > 0))
            {
                result = PalSignalSemaphores(
                    submitInfo.signalSemaphoreCount,
                    submitInfo.pSignalSemaphores,
                    nullptr,
                    (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->signalSemaphoreCount          : 0),
                    (pDeviceGroupInfo != nullptr ? pDeviceGroupInfo->pSignalSemaphoreDeviceIndices : nullptr));
            }

        }
    }
    return result;
}

// =====================================================================================================================
// Wait for a queue to go idle
VkResult Queue::WaitIdle(void)
{
    VK_ASSERT(m_pPalQueues != nullptr);

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        PalQueue(deviceIdx)->WaitIdle();
    }

    // Pal::IQueue::WaitIdle returns void. We have no errors to produce here.
    return VK_SUCCESS;
}

// =====================================================================================================================
// Signal a queue semaphore
// If semaphoreCount > semaphoreDeviceIndicesCount, the last device index will be used for the remaining semaphores.
// This can be used with semaphoreDeviceIndicesCount = 1 to signal all semaphores from the same device.
VkResult Queue::PalSignalSemaphores(
    uint32_t            semaphoreCount,
    const VkSemaphore*  pSemaphores,
    const uint64_t*     pSemaphoreValues,
    const uint32_t      semaphoreDeviceIndicesCount,
    const uint32_t*     pSemaphoreDeviceIndices)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) &&
                             pDevModeMgr->IsQueueTimingActive(m_pDevice) &&
                             settings.devModeSemaphoreQueueTimingEnable);
#else
    bool timedQueueEvents = false;
#endif

    Pal::Result palResult = Pal::Result::Success;
    uint32_t    deviceIdx = DefaultDeviceIndex;

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        if (i < semaphoreDeviceIndicesCount)
        {
            VK_ASSERT(pSemaphoreDeviceIndices != nullptr);
            deviceIdx = pSemaphoreDeviceIndices[i];
        }

        VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

        Semaphore* pVkSemaphore = Semaphore::ObjectFromHandle(pSemaphores[i]);
        Pal::IQueueSemaphore* pPalSemaphore = pVkSemaphore->PalSemaphore(deviceIdx);
        uint64_t              pointValue    = 0;

        if (pVkSemaphore->IsTimelineSemaphore())
        {
            if (pSemaphoreValues == nullptr)
            {
                palResult = Pal::Result::ErrorInvalidPointer;
                break;
            }
            else
            {
                VK_ASSERT(pSemaphoreValues[i] != 0);
                pointValue = pSemaphoreValues[i];
            }
        }

        if (timedQueueEvents == false)
        {
            if (pVkSemaphore->PalTemporarySemaphore(deviceIdx))
            {
                pPalSemaphore = pVkSemaphore->PalTemporarySemaphore(deviceIdx);
            }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
            palResult = PalQueue(deviceIdx)->SignalQueueSemaphore(pPalSemaphore, pointValue);
#else
            palResult = PalQueue(deviceIdx)->SignalQueueSemaphore(pPalSemaphore);
#endif
        }
        else
        {
#if ICD_GPUOPEN_DEVMODE_BUILD
            palResult = pDevModeMgr->TimedSignalQueueSemaphore(deviceIdx, this, pSemaphores[i], pointValue,
                                                               pPalSemaphore);
#else
            VK_NEVER_CALLED();

            palResult = Pal::Result::ErrorUnknown;
#endif
        }
    }

    return  (palResult == Pal::Result::ErrorUnknown) ? VK_ERROR_DEVICE_LOST : PalToVkResult(palResult);
}

// =====================================================================================================================
// Wait for a queue semaphore to become signaled
// If semaphoreCount > semaphoreDeviceIndicesCount, the last device index will be used for the remaining semaphores.
// This can be used with semaphoreDeviceIndicesCount = 1 to signal all semaphores from the same device.
VkResult Queue::PalWaitSemaphores(
    uint32_t            semaphoreCount,
    const VkSemaphore*  pSemaphores,
    const uint64_t*     pSemaphoreValues,
    const uint32_t      semaphoreDeviceIndicesCount,
    const uint32_t*     pSemaphoreDeviceIndices)
{
    Pal::Result palResult = Pal::Result::Success;
    uint32_t    deviceIdx = DefaultDeviceIndex;

#if ICD_GPUOPEN_DEVMODE_BUILD
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) &&
                             pDevModeMgr->IsQueueTimingActive(m_pDevice) &&
                             settings.devModeSemaphoreQueueTimingEnable);
#else
    bool timedQueueEvents = false;
#endif

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        Semaphore*  pSemaphore              = Semaphore::ObjectFromHandle(pSemaphores[i]);
        Pal::IQueueSemaphore* pPalSemaphore = nullptr;
        uint64_t              pointValue    = 0;

        if (pSemaphore->IsTimelineSemaphore())
        {
            if (pSemaphoreValues == nullptr)
            {
                palResult = Pal::Result::ErrorInvalidPointer;
                break;
            }
            else
            {
                VK_ASSERT(pSemaphoreValues[i] != 0);
                pointValue = pSemaphoreValues[i];
            }
        }

        if (i < semaphoreDeviceIndicesCount)
        {
            VK_ASSERT(pSemaphoreDeviceIndices != nullptr);
            deviceIdx = pSemaphoreDeviceIndices[i];
        }

        VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

        // Wait for the temporary semaphore.
        if (pSemaphore->PalTemporarySemaphore(deviceIdx) != nullptr)
        {
            pPalSemaphore = pSemaphore->PalTemporarySemaphore(deviceIdx);
            pSemaphore->ClearPalTemporarySemaphore();
        }
        else
        {
            pPalSemaphore = pSemaphore->PalSemaphore(deviceIdx);
        }

        if (pPalSemaphore != nullptr)
        {
            if (timedQueueEvents == false)
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
                palResult = PalQueue(deviceIdx)->WaitQueueSemaphore(pPalSemaphore, pointValue);
#else
                palResult = PalQueue(deviceIdx)->WaitQueueSemaphore(pPalSemaphore);
#endif
            }
            else
            {
#if ICD_GPUOPEN_DEVMODE_BUILD
                palResult = pDevModeMgr->TimedWaitQueueSemaphore(deviceIdx, this, pSemaphores[i], pointValue,
                                                                 pPalSemaphore);
#else
                VK_NEVER_CALLED();

                palResult = Pal::Result::ErrorUnknown;
#endif
            }
        }

    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Update present flip status
VkResult Queue::UpdateFlipStatus(
    const Pal::PresentSwapChainInfo* pPresentInfo,
    const SwapChain*                 pSwapChain)
{
    bool isOwner = false;
    Pal::IDevice* pPalDevice = m_pDevice->PalDevice(DefaultDeviceIndex);
    uint32_t vidPnSourceId = pSwapChain->GetFullscreenMgr()->GetVidPnSourceId();

    Pal::Result palResult = pPalDevice->GetFlipStatus(vidPnSourceId, &m_flipStatus.flipFlags, &isOwner);
    if (palResult == Pal::Result::Success)
    {
        m_flipStatus.isValid     = true;
        m_flipStatus.isFlipOwner = isOwner;
    }
    else
    {
        memset(&m_flipStatus, 0, sizeof(m_flipStatus));
    }

    palResult = pPalDevice->PollFullScreenFrameMetadataControl(vidPnSourceId, &m_palFrameMetadataControl);
    VK_ASSERT(palResult == Pal::Result::Success);

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Return true if pace present is needed, should sync flip (timer queue hold present queue) if pSyncFlip is true
bool Queue::NeedPacePresent(
    Pal::PresentSwapChainInfo* pPresentInfo,
    const SwapChain*           pSwapChain,
    bool*                      pSyncFlip,
    bool*                      pPostFrameTimerSubmission)
{
    VkResult result = VK_SUCCESS;

    return false;
}

// =====================================================================================================================
// Present a swap chain image
VkResult Queue::Present(
    const VkPresentInfoKHR* pPresentInfo)
{
    uint32_t presentationDeviceIdx = 0;

    const VkPresentInfoKHR* pVkInfo = nullptr;

    {
        union
        {
            const VkStructHeader*              pHeader;
            const VkPresentInfoKHR*            pVkPresentInfoKHR;
            const VkDeviceGroupPresentInfoKHR* pVkDeviceGroupPresentInfoKHR;
        };

        for (pVkPresentInfoKHR = pPresentInfo; pHeader != nullptr; pHeader = pHeader->pNext)
        {
            switch (static_cast<int32_t>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
                pVkInfo = pVkPresentInfoKHR;
                break;

            case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR:
            {
                VK_ASSERT(pVkDeviceGroupPresentInfoKHR->swapchainCount == 1);
                const uint32_t deviceMask = *pVkDeviceGroupPresentInfoKHR->pDeviceMasks;
                VK_ASSERT(Util::CountSetBits(deviceMask) == 1);
                Util::BitMaskScanForward(&presentationDeviceIdx, deviceMask);
                break;
            }
            default:
                // Skip any unknown extension structures
                break;
            }
        }
    }

    VkResult result = VK_SUCCESS;

    if (pPresentInfo == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (pPresentInfo->waitSemaphoreCount > 0)
    {
        result = PalWaitSemaphores(
            pPresentInfo->waitSemaphoreCount,
            pPresentInfo->pWaitSemaphores,
            nullptr,
            0,
            nullptr);
    }

    // Get presentable image object
    for (uint32_t curSwapchain = 0; curSwapchain < pPresentInfo->swapchainCount; curSwapchain++)
    {
        // Get the swap chain
        SwapChain* pSwapChain = SwapChain::ObjectFromHandle(pPresentInfo->pSwapchains[curSwapchain]);

        // Get the presentable image index
        const uint32_t imageIndex = pPresentInfo->pImageIndices[curSwapchain];

        // Fill in present information
        Pal::PresentSwapChainInfo presentInfo = {};

        // For MGPU, the swapchain and device properties might perform software composition and return
        // a different presentation device for the present of the intermediate surface.
        Pal::IQueue* pPresentQueue = pSwapChain->PrePresent(presentationDeviceIdx, imageIndex, &presentInfo, this);

        // Notify gpuopen developer mode that we're about to present (frame-end boundary)
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameEnd(this, true);
        }
#endif

        bool syncFlip = false;
        bool postFrameTimerSubmission = false;
        bool needFramePacing = NeedPacePresent(&presentInfo, pSwapChain, &syncFlip, &postFrameTimerSubmission);
        const Pal::IGpuMemory* pGpuMemory =
            pSwapChain->GetPresentableImageMemory(imageIndex)->PalMemory(DefaultDeviceIndex);

        result = NotifyFlipMetadataBeforePresent(presentationDeviceIdx, pPresentQueue, &presentInfo, pGpuMemory);
        if (result != VK_SUCCESS)
        {
            break;
        }

        // Perform the actual present
        Pal::Result palResult = pPresentQueue->PresentSwapChain(presentInfo);

        result = NotifyFlipMetadataAfterPresent(presentationDeviceIdx, pPresentQueue, &presentInfo);

        if (result != VK_SUCCESS)
        {
            break;
        }

        // Notify swap chain that a present occurred
        pSwapChain->PostPresent(presentInfo, &palResult);

        // Notify gpuopen developer mode that a present occurred (frame-begin boundary)
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->NotifyFrameBegin(this, true);
        }
#endif

        VkResult curResult = PalToVkResult(palResult);

        if (pPresentInfo->pResults)
        {
            pPresentInfo->pResults[curSwapchain] = curResult;
        }

        // We need to keep track of the most severe error code reported and
        // make sure that's the one we ultimately return
        if ((curResult == VK_ERROR_DEVICE_LOST) ||
            (curResult == VK_ERROR_SURFACE_LOST_KHR && result != VK_ERROR_DEVICE_LOST) ||
            (curResult == VK_ERROR_OUT_OF_DATE_KHR && result != VK_ERROR_SURFACE_LOST_KHR) ||
            (curResult == VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR &&
                result != VK_ERROR_SURFACE_LOST_KHR))
        {
            result = curResult;
        }
        else if (curResult != VK_SUCCESS)
        {
            VK_ASSERT(!"Unexpected VK ERROR set, check spec to ensure it is valid.");

            result = VK_ERROR_DEVICE_LOST;
        }
    }

    return result;
}

// =====================================================================================================================
// Adds an entry to the remap range array.
VkResult Queue::AddVirtualRemapRange(
    uint32_t           resourceDeviceIndex,
    Pal::IGpuMemory*   pVirtualGpuMem,
    VkDeviceSize       virtualOffset,
    Pal::IGpuMemory*   pRealGpuMem,
    VkDeviceSize       realOffset,
    VkDeviceSize       size,
    VirtualRemapState* pRemapState)
{
    VkResult result = VK_SUCCESS;

    VK_ASSERT(pRemapState->rangeCount < pRemapState->maxRangeCount);

    Pal::VirtualMemoryRemapRange* pRemapRange = &pRemapState->pRanges[pRemapState->rangeCount++];

    if (m_pDevice->VkPhysicalDevice(resourceDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
    {
        pRemapRange->virtualAccessMode = Pal::VirtualGpuMemAccessMode::ReadZero;
    }
    else
    {
        pRemapRange->virtualAccessMode = Pal::VirtualGpuMemAccessMode::Undefined;
    }

    pRemapRange->pVirtualGpuMem     = pVirtualGpuMem;
    pRemapRange->virtualStartOffset = virtualOffset;
    pRemapRange->pRealGpuMem        = pRealGpuMem;
    pRemapRange->realStartOffset    = realOffset;
    pRemapRange->size               = size;

    // If we've hit our limit of batched remaps, send them to PAL and reset
    if (pRemapState->rangeCount >= pRemapState->maxRangeCount)
    {
        result = CommitVirtualRemapRanges(resourceDeviceIndex, nullptr, pRemapState);
    }

    return result;
}

// =====================================================================================================================
// Sends any pending virtual remap ranges to PAL and resets the state.  This function also handles remap fence
// signaling if requested.
VkResult Queue::CommitVirtualRemapRanges(
    uint32_t           deviceIndex,
    Pal::IFence*       pFence,
    VirtualRemapState* pRemapState)
{
    Pal::Result result = Pal::Result::Success;

    if (pRemapState->rangeCount > 0)
    {
        result = PalQueue(deviceIndex)->RemapVirtualMemoryPages(
            pRemapState->rangeCount,
            pRemapState->pRanges,
            true,
            pFence);

        pRemapState->rangeCount = 0;
    }
    else if (pFence != nullptr)
    {
        Pal::SubmitInfo submitInfo = {};

        submitInfo.pFence = pFence;

        result = PalQueue(deviceIndex)->Submit(submitInfo);
    }

    return (result == Pal::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Generate virtual remap entries for a single bind sparse info record
VkResult Queue::BindSparseEntry(
    const VkBindSparseInfo& bindInfo,
    uint32_t                resourceDeviceIndex,
    uint32_t                memoryDeviceIndex,
    VkDeviceSize            prtTileSize,
    VirtualRemapState*      pRemapState)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t j = 0; j < bindInfo.bufferBindCount; ++j)
    {
        const auto& bufBindInfo = bindInfo.pBufferBinds[j];
        const Buffer& buffer = *Buffer::ObjectFromHandle(bufBindInfo.buffer);

        VK_ASSERT(buffer.IsSparse());

        Pal::IGpuMemory* pVirtualGpuMem = buffer.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < bufBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = bufBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            VK_ASSERT(bind.flags == 0);

            result = AddVirtualRemapRange(
                resourceDeviceIndex,
                pVirtualGpuMem,
                bind.resourceOffset,
                pRealGpuMem,
                bind.memoryOffset,
                bind.size,
                pRemapState);

            if (result != VK_SUCCESS)
            {
                goto End;
            }
        }
    }

    for (uint32_t j = 0; j < bindInfo.imageOpaqueBindCount; ++j)
    {
        const auto& imgBindInfo = bindInfo.pImageOpaqueBinds[j];
        const Image& image = *Image::ObjectFromHandle(imgBindInfo.image);

        VK_ASSERT(image.IsSparse());

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = imgBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            result = AddVirtualRemapRange(
                resourceDeviceIndex,
                pVirtualGpuMem,
                bind.resourceOffset,
                pRealGpuMem,
                bind.memoryOffset,
                bind.size,
                pRemapState);

            if (result != VK_SUCCESS)
            {
                goto End;
            }
        }
    }

    for (uint32_t j = 0; j < bindInfo.imageBindCount; ++j)
    {
        const auto& imgBindInfo = bindInfo.pImageBinds[j];
        const Image& image = *Image::ObjectFromHandle(imgBindInfo.image);

        VK_ASSERT(image.IsSparse());

        const VkExtent3D& tileSize = image.GetTileSize();

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(resourceDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseImageMemoryBind& bind = imgBindInfo.pBinds[k];

            VK_ASSERT(bind.flags == 0);

            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(resourceDeviceIndex, memoryDeviceIndex);
            }

            // Get the subresource layout to be able to figure out its offset
            Pal::Result       palResult;
            Pal::SubresLayout subResLayout = {};
            Pal::SubresId     subResId     = {};

            subResId.aspect     = VkToPalImageAspectSingle(bind.subresource.aspectMask);
            subResId.mipLevel   = bind.subresource.mipLevel;
            subResId.arraySlice = bind.subresource.arrayLayer;

            palResult = image.PalImage(resourceDeviceIndex)->GetSubresourceLayout(subResId, &subResLayout);

            if (palResult != Pal::Result::Success)
            {
                goto End;
            }

            // Calculate subresource row and depth pitch in tiles
            // In Gfx9, the tiles within same mip level may not continuous thus we have to take
            // the mipChainPitch into account when calculate the offset of next tile.
            VkDeviceSize prtTileRowPitch   = subResLayout.rowPitch * tileSize.height * tileSize.depth;
            VkDeviceSize prtTileDepthPitch = subResLayout.depthPitch * tileSize.depth;

            // tileSize is in texels, prtTileRowPitch and prtTileDepthPitch shall be adjusted for compressed
            // formats. depth of blockDim will always be 1 so skip the adjustment.
            const VkFormat aspectFormat = vk::Formats::GetAspectFormat(image.GetFormat(), bind.subresource.aspectMask);
            const Pal::ChNumFormat palAspectFormat = VkToPalFormat(aspectFormat).format;
            if (Pal::Formats::IsBlockCompressed(palAspectFormat))
            {
                Pal::Extent3d blockDim = Pal::Formats::CompressedBlockDim(palAspectFormat);
                VK_ASSERT(blockDim.depth == 1);
                prtTileRowPitch /= blockDim.height;
            }

            // Calculate the offsets in tiles
            const VkOffset3D offsetInTiles =
            {
                bind.offset.x / static_cast<int32_t>(tileSize.width),
                bind.offset.y / static_cast<int32_t>(tileSize.height),
                bind.offset.z / static_cast<int32_t>(tileSize.depth)
            };

            // Calculate the extents in tiles
            const VkExtent3D extentInTiles =
            {
                Util::Pow2Align(bind.extent.width,  tileSize.width) / tileSize.width,
                Util::Pow2Align(bind.extent.height, tileSize.height) / tileSize.height,
                Util::Pow2Align(bind.extent.depth,  tileSize.depth) / tileSize.depth
            };

            // Calculate byte size to remap per row
            VkDeviceSize sizePerRow = extentInTiles.width * prtTileSize;
            VkDeviceSize realOffset = bind.memoryOffset;

            for (uint32_t tileZ = 0; tileZ < extentInTiles.depth; ++tileZ)
            {
                for (uint32_t tileY = 0; tileY < extentInTiles.height; ++tileY)
                {
                    VkDeviceSize virtualOffset =
                        subResLayout.offset +
                        offsetInTiles.x * prtTileSize +
                        (offsetInTiles.y + tileY) * prtTileRowPitch +
                        (offsetInTiles.z + tileZ) * prtTileDepthPitch;

                    result = AddVirtualRemapRange(
                        resourceDeviceIndex,
                        pVirtualGpuMem,
                        virtualOffset,
                        pRealGpuMem,
                        realOffset,
                        sizePerRow,
                        pRemapState);

                    if (result != VK_SUCCESS)
                    {
                        goto End;
                    }

                    realOffset += sizePerRow;
                }
            }
        }
    }

End:
    return result;
}

// =====================================================================================================================
// Peek the resource and memory device indices from a chained VkDeviceGroupBindSparseInfo structure.
static void PeekDeviceGroupBindSparseDeviceIndices(
    const VkBindSparseInfo& bindInfo,
    uint32_t*               pResourceDeviceIndex,
    uint32_t*               pMemoryDeviceIndex)

{
    union
    {
        const VkStructHeader*               pHeader;
        const VkDeviceGroupBindSparseInfo*  pDeviceGroupBindSparseInfo;
    };

    for (pHeader  = static_cast<const VkStructHeader*>(bindInfo.pNext);
         pHeader != nullptr;
         pHeader  = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
            case VK_STRUCTURE_TYPE_DEVICE_GROUP_BIND_SPARSE_INFO:
            {
                if (pResourceDeviceIndex != nullptr)
                {
                    *pResourceDeviceIndex = pDeviceGroupBindSparseInfo->resourceDeviceIndex;
                }
                if (pMemoryDeviceIndex != nullptr)
                {
                    *pMemoryDeviceIndex = pDeviceGroupBindSparseInfo->memoryDeviceIndex;
                }

                break;
            }
            default:
                // Skip any unknown extension structures
                break;
        }
    }
}
// =====================================================================================================================
// Update sparse bindings.
VkResult Queue::BindSparse(
    uint32_t                bindInfoCount,
    const VkBindSparseInfo* pBindInfo,
    VkFence                 fence)
{
    VkResult result = VK_SUCCESS;

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Initialize state to track batches of sparse bind calls
    VirtualRemapState remapState = {};

    // Max number of sparse bind operations per batch
    constexpr uint32_t MaxVirtualRemapRangesPerBatch = 1024;

    remapState.maxRangeCount =
        Util::Min(
            MaxVirtualRemapRangesPerBatch,
            static_cast<uint32_t>(m_pStackAllocator->Remaining() / sizeof(Pal::VirtualMemoryRemapRange)));

    // Allocate temp memory for one batch of remaps
    remapState.pRanges = virtStackFrame.AllocArray<Pal::VirtualMemoryRemapRange>(remapState.maxRangeCount);

    if (remapState.pRanges == nullptr)
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Fence will be signalled after the remaps
    uint32_t signalFenceDeviceMask = 0;

    for (uint32_t i = 0; (i < bindInfoCount) && (result == VK_SUCCESS); ++i)
    {
        const bool  lastEntry               = (i == (bindInfoCount - 1));
        const auto& bindInfo                = pBindInfo[i];
        uint32_t    resourceDeviceIndex     = DefaultDeviceIndex;
        uint32_t    memoryDeviceIndex       = DefaultDeviceIndex;
        uint32_t    nextResourceDeviceIndex = DefaultDeviceIndex;
        PeekDeviceGroupBindSparseDeviceIndices(bindInfo, &resourceDeviceIndex, &memoryDeviceIndex);

        if (!lastEntry)
        {
            PeekDeviceGroupBindSparseDeviceIndices(pBindInfo[i + 1], &nextResourceDeviceIndex, nullptr);
        }

        // Keep track of the PAL devices that need to signal the fence
        signalFenceDeviceMask |= (1 << resourceDeviceIndex);

        if (bindInfo.waitSemaphoreCount > 0)
        {
            result = PalWaitSemaphores(
                    bindInfo.waitSemaphoreCount,
                    bindInfo.pWaitSemaphores,
                    nullptr,
                    1,  // number of device indices
                    &resourceDeviceIndex);
        }

        if (result == VK_SUCCESS)
        {
            // Byte size of a PRT sparse tile
            const VkDeviceSize prtTileSize =
                m_pDevice->VkPhysicalDevice(resourceDeviceIndex)->PalProperties().imageProperties.prtTileSize;

            result = BindSparseEntry(
                bindInfo,
                resourceDeviceIndex,
                memoryDeviceIndex,
                prtTileSize,
                &remapState);
        }

        // Commit any batched remap operations immediately if:
        // - this is the last batch,
        // - resourceDeviceIndex is going to change,
        // - the app is requesting us to signal a queue semaphore.
        if (lastEntry || (nextResourceDeviceIndex != resourceDeviceIndex) || (bindInfo.signalSemaphoreCount > 0))
        {
            // Commit any remaining remaps
            if (result == VK_SUCCESS)
            {
                result = CommitVirtualRemapRanges(resourceDeviceIndex, nullptr, &remapState);
            }

            // Signal any semaphores depending on the preceding remap operations
            if (result == VK_SUCCESS)
            {
                result = PalSignalSemaphores(
                    bindInfo.signalSemaphoreCount,
                    bindInfo.pSignalSemaphores,
                    nullptr,
                    1,  // number of device indices
                    &resourceDeviceIndex);
            }
        }
    }

    // Clean up
    VK_ASSERT((remapState.rangeCount == 0) || (result != VK_SUCCESS));

    if ((result == VK_SUCCESS) && (fence != VK_NULL_HANDLE))
    {
        // Signal the fence in the devices that have binding request.
        // If there is no bindInfo, we just signal the fence in the DeviceDeviceIndex.
        if (signalFenceDeviceMask == 0)
        {
            signalFenceDeviceMask = 1 << DefaultDeviceIndex;
        }

        Pal::SubmitInfo submitInfo = {};

        Fence* pFence = Fence::ObjectFromHandle(fence);

        utils::IterateMask deviceGroup(signalFenceDeviceMask);

        while (result == VK_SUCCESS && deviceGroup.Iterate())
        {
            const uint32_t deviceIndex = deviceGroup.Index();
            submitInfo.pFence = pFence->PalFence(deviceIndex);

            // set the active device mask for the fence.
            // the following fence wait would only be applied to the fence one the active device index.
            // the active device index would be cleared in the reset.
            pFence->SetActiveDevice(deviceIndex);

            result = PalToVkResult(PalQueue(deviceIndex)->Submit(submitInfo));
        }
    }

    virtStackFrame.FreeArray(remapState.pRanges);

    return result;
}

/**
 ***********************************************************************************************************************
 * C-Callable entry points start here. These entries go in the dispatch table(s).
 ***********************************************************************************************************************
 */

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
     return ApiQueue::ObjectFromHandle(queue)->Submit(
        submitCount,
        pSubmits,
        fence);
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(
    VkQueue                                     queue)
{
    return ApiQueue::ObjectFromHandle(queue)->WaitIdle();
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueBindSparse(
    VkQueue                                     queue,
    uint32_t                                    bindInfoCount,
    const VkBindSparseInfo*                     pBindInfo,
    VkFence                                     fence)
{
    return ApiQueue::ObjectFromHandle(queue)->BindSparse(bindInfoCount, pBindInfo, fence);
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue                                      queue,
    const VkPresentInfoKHR*                      pPresentInfo)
{
    return ApiQueue::ObjectFromHandle(queue)->Present(pPresentInfo);
}

} // namespace entry

} // namespace vk
