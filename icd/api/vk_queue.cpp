/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  vk_queue.cpp
 * @brief Contains implementation of Vulkan query pool.
 ***********************************************************************************************************************
 */

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_display.h"
#include "include/vk_fence.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_image.h"
#include "include/vk_object.h"
#include "include/vk_queue.h"
#include "include/vk_buffer.h"
#include "include/vk_semaphore.h"
#include "include/vk_swapchain.h"

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
    Pal::IQueue**           pPalQueues,
    VirtualStackAllocator*  pStackAllocator)
    :
    m_pDevice(pDevice),
    m_queueFamilyIndex(queueFamilyIndex),
    m_queueIndex(queueIndex),
    m_pDevModeMgr(pDevice->VkInstance()->GetDevModeMgr()),
    m_pStackAllocator(pStackAllocator),
    m_pDummyCmdBuffer(nullptr)
{
    memcpy(m_pPalQueues, pPalQueues, sizeof(pPalQueues[0]) * pDevice->NumPalDevices());
    memset(&m_palFrameMetadataControl, 0, sizeof(Pal::PerSourceFrameMetadataControl));

}

// =====================================================================================================================
Queue::~Queue()
{
    if (m_pDummyCmdBuffer != nullptr)
    {
        m_pDummyCmdBuffer->Destroy();
        m_pDevice->VkInstance()->FreeMem(m_pDummyCmdBuffer);
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

    Pal::CmdBufferCreateInfo palCreateInfo = {};
    palCreateInfo.pCmdAllocator = m_pDevice->GetSharedCmdAllocator(DefaultDeviceIndex);
    palCreateInfo.queueType     = m_pDevice->GetQueueFamilyPalQueueType(m_queueFamilyIndex);
    palCreateInfo.engineType    = m_pDevice->GetQueueFamilyPalEngineType(m_queueFamilyIndex);

    Pal::IDevice* const pPalDevice = m_pDevice->PalDevice();
    const size_t palSize = pPalDevice->GetCmdBufferSize(palCreateInfo, &palResult);
    if (palResult == Pal::Result::Success)
    {
        void* pMemory = m_pDevice->VkInstance()->AllocMem(palSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
        if (pMemory != nullptr)
        {
            palResult = pPalDevice->CreateCmdBuffer(palCreateInfo, pMemory, &m_pDummyCmdBuffer);
            if (palResult == Pal::Result::Success)
            {
                Pal::CmdBufferBuildInfo buildInfo = {};
                buildInfo.flags.optimizeExclusiveSubmit = 1;
                palResult = m_pDummyCmdBuffer->Begin(buildInfo);
                if (palResult == Pal::Result::Success)
                {
                    palResult = m_pDummyCmdBuffer->End();
                }
            }
        }
        else
        {
            palResult = Pal::Result::ErrorOutOfMemory;
        }
    }

    return PalToVkResult(palResult);
}

// =====================================================================================================================
// Submit a dummy command buffer with associated command buffer info to KMD for FRTC/TurboSync/DVR features
VkResult Queue::NotifyFlipMetadata(
    const Pal::IGpuMemory*       pGpuMemory,
    FullscreenFrameMetadataFlags flags)
{
    VkResult result = VK_SUCCESS;
    if (m_pDummyCmdBuffer == nullptr)
    {
        result = CreateDummyCmdBuffer();
        VK_ASSERT(result == VK_SUCCESS);
    }

    if (m_pDummyCmdBuffer != nullptr)
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
            submitInfo.ppCmdBuffers    = &m_pDummyCmdBuffer;
            submitInfo.pCmdBufInfoList = &cmdBufInfo;

            result = PalToVkResult(m_pPalQueues[DefaultDeviceIndex]->Submit(submitInfo));
            VK_ASSERT(result == VK_SUCCESS);
        }
    }

    return result;
}

// =====================================================================================================================
// Submit command buffer info with frameEndFlag and primaryHandle before frame present
VkResult Queue::NotifyFlipMetadataBeforePresent(
    const Pal::PresentSwapChainInfo* pPresentInfo,
    const Pal::IGpuMemory*           pGpuMemory)
{
    VkResult result = VK_SUCCESS;
    return result;
}

// =====================================================================================================================
// Submit command buffer info with frameBeginFlag after frame present
VkResult Queue::NotifyFlipMetadataAfterPresent(
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

        submitInfo.pFence = pFence->PalFence();
        palResult = PalQueue()->Submit(submitInfo);

        result = PalToVkResult(palResult);
    }
    else
    {
        for (uint32_t submitIdx = 0; (submitIdx < submitCount) && (result == VK_SUCCESS); ++submitIdx)
        {
            const VkSubmitInfo& submitInfo = pSubmits[submitIdx];
            const VkDeviceGroupSubmitInfoKHX* pDeviceGroupInfo = nullptr;
            {
                union
                {
                    const VkStructHeader*                          pHeader;
                    const VkSubmitInfo*                            pVkSubmitInfo;
                    const VkDeviceGroupSubmitInfoKHX*              pVkDeviceGroupSubmitInfoKHX;
                };

                for (pVkSubmitInfo = &submitInfo; pHeader != nullptr; pHeader = pHeader->pNext)
                {
                    switch (static_cast<int32_t>(pHeader->sType))
                    {
                    case VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO_KHX:
                        pDeviceGroupInfo = pVkDeviceGroupSubmitInfoKHX;
                        break;
                    default:
                        // Skip any unknown extension structures
                        break;
                    }
                }
            }

            if ((result == VK_SUCCESS) && (submitInfo.waitSemaphoreCount > 0))
            {
                result = PalWaitSemaphores(submitInfo.waitSemaphoreCount,
                                           submitInfo.pWaitSemaphores,
                                           pDeviceGroupInfo);
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

                if ((palSubmitInfo.cmdBufferCount > 0) || (palSubmitInfo.pFence != nullptr))
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
                result = PalSignalSemaphores(submitInfo.signalSemaphoreCount,
                                             submitInfo.pSignalSemaphores,
                                             pDeviceGroupInfo);
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
VkResult Queue::PalSignalSemaphores(
    uint32_t                          semaphoreCount,
    const VkSemaphore*                pSemaphores,
    const VkDeviceGroupSubmitInfoKHX* pDeviceGroupInfo)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) && pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif

    Pal::Result palResult = Pal::Result::Success;

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        const uint32_t deviceIdx = (pDeviceGroupInfo != nullptr) ?
                                    pDeviceGroupInfo->pSignalSemaphoreDeviceIndices[i] : DefaultDeviceIndex;

        VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

        Semaphore* pVkSemaphore = Semaphore::ObjectFromHandle(pSemaphores[i]);
        Pal::IQueueSemaphore* pPalSemaphore = pVkSemaphore->PalSemaphore();

        if (timedQueueEvents == false)
        {
            if (pVkSemaphore->PalTemporarySemaphore())
            {
                pPalSemaphore = pVkSemaphore->PalTemporarySemaphore();
            }

            palResult = PalQueue(deviceIdx)->SignalQueueSemaphore(pPalSemaphore);
        }
        else
        {
#if ICD_GPUOPEN_DEVMODE_BUILD
            palResult = pDevModeMgr->TimedSignalQueueSemaphore(deviceIdx, this, pSemaphores[i], pPalSemaphore);
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
VkResult Queue::PalWaitSemaphores(
    uint32_t                          semaphoreCount,
    const VkSemaphore*                pSemaphores,
    const VkDeviceGroupSubmitInfoKHX* pDeviceGroupInfo)
{
    Pal::Result palResult = Pal::Result::Success;

#if ICD_GPUOPEN_DEVMODE_BUILD
    DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    bool timedQueueEvents = ((pDevModeMgr != nullptr) && pDevModeMgr->IsQueueTimingActive(m_pDevice));
#else
    bool timedQueueEvents = false;
#endif

    for (uint32_t i = 0; (i < semaphoreCount) && (palResult == Pal::Result::Success); ++i)
    {
        Semaphore*  pSemaphore              = Semaphore::ObjectFromHandle(pSemaphores[i]);
        Pal::IQueueSemaphore* pPalSemaphore = nullptr;

        const uint32_t deviceIdx = (pDeviceGroupInfo != nullptr) ?
                                    pDeviceGroupInfo->pWaitSemaphoreDeviceIndices[i] : DefaultDeviceIndex;

        VK_ASSERT(deviceIdx < m_pDevice->NumPalDevices());

        // Wait for the temporary semaphore.
        if (pSemaphore->PalTemporarySemaphore() != nullptr)
        {
            pPalSemaphore = pSemaphore->PalTemporarySemaphore();
            pSemaphore->SetPalTemporarySemaphore(nullptr);
        }
        else
        {
            pPalSemaphore = pSemaphore->PalSemaphore();
        }

        if (pPalSemaphore != nullptr)
        {
            if (timedQueueEvents == false)
            {
                palResult = PalQueue(deviceIdx)->WaitQueueSemaphore(pPalSemaphore);
            }
            else
            {
#if ICD_GPUOPEN_DEVMODE_BUILD
                palResult = pDevModeMgr->TimedWaitQueueSemaphore(deviceIdx, this, pSemaphores[i], pPalSemaphore);
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
    Pal::IDevice* pPalDevice = m_pDevice->PalDevice();
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
            const VkDeviceGroupPresentInfoKHX* pVkDeviceGroupPresentInfoKHX;
        };

        for (pVkPresentInfoKHR = pPresentInfo; pHeader != nullptr; pHeader = pHeader->pNext)
        {
            switch (static_cast<int32_t>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PRESENT_INFO_KHR:
                pVkInfo = pVkPresentInfoKHR;
                break;

            case VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHX:
            {
                // TODO: SWDEV-120359 - We need to handle multiple swapchains
                VK_ASSERT(pVkDeviceGroupPresentInfoKHX->swapchainCount == 1);
                const uint32_t deviceMask = *pVkDeviceGroupPresentInfoKHX->pDeviceMasks;
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

        result = pSwapChain->GetPresentInfo(presentationDeviceIdx, imageIndex, &presentInfo);

        // Notify gpuopen developer mode that we're about to present (frame-end boundary)
#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevice->VkInstance()->GetDevModeMgr() != nullptr)
        {
            m_pDevice->VkInstance()->GetDevModeMgr()->PrePresent(this);
        }
#endif

        bool syncFlip = false;
        bool postFrameTimerSubmission = false;
        bool needFramePacing = NeedPacePresent(&presentInfo, pSwapChain, &syncFlip, &postFrameTimerSubmission);
        const Pal::IGpuMemory* pGpuMemory = pSwapChain->GetPresentableImageMemory(imageIndex)->PalMemory();
        result = NotifyFlipMetadataBeforePresent(&presentInfo, pGpuMemory);
        if (result != VK_SUCCESS)
        {
            break;
        }

        // Perform the actual present
        Pal::Result palResult = PalQueue(presentationDeviceIdx)->PresentSwapChain(presentInfo);

        result = NotifyFlipMetadataAfterPresent(&presentInfo);
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
            m_pDevice->VkInstance()->GetDevModeMgr()->PostPresent(this);
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

    if (m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetPrtFeatures() & Pal::PrtFeatureStrictNull)
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
        result = CommitVirtualRemapRanges(nullptr, pRemapState);
    }

    return result;
}

// =====================================================================================================================
// Sends any pending virtual remap ranges to PAL and resets the state.  This function also handles remap fence
// signaling if requested.
VkResult Queue::CommitVirtualRemapRanges(
    Pal::IFence*       pFence,
    VirtualRemapState* pRemapState)
{
    Pal::Result result = Pal::Result::Success;

    if (pRemapState->rangeCount > 0)
    {
        result = PalQueue(DefaultDeviceIndex)->RemapVirtualMemoryPages(
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

        result = PalQueue(DefaultDeviceIndex)->Submit(submitInfo);
    }

    return (result == Pal::Result::Success) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

// =====================================================================================================================
// Generate virtual remap entries for a single bind sparse info record
VkResult Queue::BindSparseEntry(
    const VkBindSparseInfo& bindInfo,
    VkDeviceSize            prtTileSize,
    VirtualRemapState*      pRemapState)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t j = 0; j < bindInfo.bufferBindCount; ++j)
    {
        const auto& bufBindInfo = bindInfo.pBufferBinds[j];
        const Buffer& buffer = *Buffer::ObjectFromHandle(bufBindInfo.buffer);

        VK_ASSERT(buffer.IsSparse());

        Pal::IGpuMemory* pVirtualGpuMem = buffer.PalMemory(DefaultDeviceIndex);

        for (uint32_t k = 0; k < bufBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = bufBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(DefaultDeviceIndex);
            }

            VK_ASSERT(bind.flags == 0);

            result = AddVirtualRemapRange(
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

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(DefaultDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseMemoryBind& bind = imgBindInfo.pBinds[k];
            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(DefaultDeviceIndex);
            }

            result = AddVirtualRemapRange(
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

        Pal::IGpuMemory* pVirtualGpuMem = image.PalMemory(DefaultDeviceIndex);

        for (uint32_t k = 0; k < imgBindInfo.bindCount; ++k)
        {
            const VkSparseImageMemoryBind& bind = imgBindInfo.pBinds[k];

            VK_ASSERT(bind.flags == 0);

            Pal::IGpuMemory* pRealGpuMem = nullptr;

            if (bind.memory != VK_NULL_HANDLE)
            {
                pRealGpuMem = Memory::ObjectFromHandle(bind.memory)->PalMemory(DefaultDeviceIndex);
            }

            // Get the subresource layout to be able to figure out its offset
            Pal::Result       palResult;
            Pal::SubresLayout subResLayout = {};
            Pal::SubresId     subResId     = {};

            subResId.aspect     = VkToPalImageAspectSingle(bind.subresource.aspectMask);
            subResId.mipLevel   = bind.subresource.mipLevel;
            subResId.arraySlice = bind.subresource.arrayLayer;

            palResult = image.PalImage(DefaultDeviceIndex)->GetSubresourceLayout(subResId, &subResLayout);

            if (palResult != Pal::Result::Success)
            {
                goto End;
            }

            // Calculate the extents of the subresource in tiles
            const VkExtent3D subresExtentInTiles =
            {
                Util::RoundUpToMultiple(Util::Max(subResLayout.paddedExtent.width, 1u),
                tileSize.width) / tileSize.width,
                Util::RoundUpToMultiple(Util::Max(subResLayout.paddedExtent.height, 1u),
                tileSize.height) / tileSize.height,
                Util::RoundUpToMultiple(Util::Max(subResLayout.paddedExtent.depth, 1u),
                tileSize.depth) / tileSize.depth
            };

            // Calculate subresource row and depth pitch in tiles
            // In Gfx9, the tiles within same mip level may not continuous thus we have to take
            // the mipChainPitch into account when calculate the offset of next tile.
            // for pre-gfx9, the blockSize.depth for none 3d resources are 0.
            uint32_t depth = subResLayout.blockSize.depth ? subResLayout.blockSize.depth : 1;
            VkDeviceSize prtTileRowPitch   = subResLayout.rowPitch * subResLayout.blockSize.height * depth;

            VkDeviceSize prtTileDepthPitch = prtTileRowPitch * subresExtentInTiles.height;

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

    // Byte size of a PRT sparse tile
    const VkDeviceSize prtTileSize =
        m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().imageProperties.prtTileSize;

    // Get the fence that should be signaled after all remap operations are completed
    Pal::IFence* pPalFence = nullptr;

    if (Fence::ObjectFromHandle(fence) != VK_NULL_HANDLE)
    {
        Fence* pFence = Fence::ObjectFromHandle(fence);
        pFence->SetActiveDevice(DefaultDeviceIndex);
        pPalFence = pFence->PalFence();
    }

    for (uint32_t i = 0; (i < bindInfoCount) && (result == VK_SUCCESS); ++i)
    {
        const bool  lastEntry = (i == (bindInfoCount - 1));
        const auto& bindInfo  = pBindInfo[i];

        if (bindInfo.waitSemaphoreCount > 0)
        {
            result = PalWaitSemaphores(
                    bindInfo.waitSemaphoreCount,
                    bindInfo.pWaitSemaphores,
                    nullptr);
        }

        if (result == VK_SUCCESS)
        {
            result = BindSparseEntry(bindInfo, prtTileSize, &remapState);
        }

        // Commit any batched remap operations immediately if either this is the last batch or the app is requesting us
        // to signal a queue semaphore when operations complete.
        if (lastEntry || (bindInfo.signalSemaphoreCount > 0))
        {
            // Commit any remaining remaps (this also signals the fence even if there are no remaining remaps)
            if (result == VK_SUCCESS)
            {
                result = CommitVirtualRemapRanges(lastEntry ? pPalFence : nullptr, &remapState);
            }

            // Signal any semaphores depending on the preceding remap operations
            if (result == VK_SUCCESS)
            {

                result = PalSignalSemaphores(
                    bindInfo.signalSemaphoreCount,
                    bindInfo.pSignalSemaphores,
                    nullptr);
            }
        }
    }

    // In cases where this function is called with no actual work, but a fence handle is given (there is a test
    // for this), signal the fence
    if ((bindInfoCount == 0) && (pPalFence != nullptr))
    {
        VK_ASSERT(remapState.rangeCount == 0);

        result = CommitVirtualRemapRanges(pPalFence, &remapState);
    }

    // Clean up
    VK_ASSERT((remapState.rangeCount == 0) || (result != VK_SUCCESS));

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
