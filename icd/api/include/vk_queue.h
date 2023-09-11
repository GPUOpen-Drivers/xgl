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
 * @file  vk_queue.h
 * @brief Declarations of queue data structures for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_QUEUE_H__
#define __VK_QUEUE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"
#include "include/virtual_stack_mgr.h"

#include "palQueue.h"

namespace Pal
{

class IQueue;

};

namespace vk
{

struct CmdBufState;
class  CmdBufferRing;
class  Device;
class  DevModeMgr;
class  ApiQueue;
class  Instance;
class  SwapChain;
class  FrtcFramePacer;
class  TurboSync;
class  SqttQueueState;
class  PhysicalDevice;
class  Memory;

// =====================================================================================================================
// A Vulkan queue.
class Queue
{
public:
    typedef VkQueue ApiType;

    Queue(
        Device*                 pDevice,
        uint32_t                queueFamilyIndex,
        uint32_t                queueIndex,
        uint32_t                queueFlags,
        Pal::IQueue**           ppPalQueues,
        Pal::IQueue**           ppPalTmzQueues,
        Pal::IQueueSemaphore**  ppPalTmzSemaphores,
        VirtualStackAllocator*  pStackAllocator,
        CmdBufferRing*          pCmdBufferRing,
        Pal::IQueue**           ppPalBackupQueues,
        Pal::IQueue**           ppPalBackupTmzQueues,
        Pal::IQueueSemaphore**  ppSwitchToPalBackupSemaphore,
        Pal::IQueueSemaphore**  ppSwitchFromPalBackupSemaphore,
        const bool              isDeviceIndependent);

    static VkResult Create(
        Device*                        pDevice,
        const VkAllocationCallbacks*   pAllocator,
        const uint32_t                 flags,
        const uint32_t                 queueFamilyIndex,
        const uint32_t                 engineIndex,
        const VkQueueGlobalPriorityKHR globalPriority,
        const uint32_t                 dedicatedComputeUnits,
        const bool                     isDeviceIndependent,
        VkQueue*                       pQueue);

    static void ConstructQueueCreateInfo(
        const PhysicalDevice&          physicalDevice,
        const uint32_t                 queueFamilyIndex,
        const uint32_t                 queueIndex,
        const uint32_t                 dedicatedComputeUnits,
        const VkQueueGlobalPriorityKHR queuePriority,
        Pal::QueueCreateInfo*          pQueueCreateInfo,
        const bool                     useComputeAsTransferQueue,
        const bool                     isTmzQueue);

    static Pal::Result CreatePalQueue(
        const PhysicalDevice&          physicalDevice,
        Pal::IDevice*                  pPalDevice,
        const uint32_t                 queueFamilyIndex,
        const uint32_t                 queueIndex,
        const uint32_t                 dedicatedComputeUnit,
        const VkQueueGlobalPriorityKHR queuePriority,
        Pal::QueueCreateInfo*          pQueueCreateInfo,
        void*                          pPalQueueMemory,
        const size_t                   palQueueMemoryOffset,
        Pal::IQueue**                  ppPalQueue,
        const wchar_t*                 pExecutableName,
        const wchar_t*                 pExecutablePath,
        const bool                     useComputeAsTransferQueue,
        const bool                     isTmzQueue);

    static VkResult GetPalQueueMemorySize(
        const Device*                  pDevice,
        const uint32_t                 queueFamilyIndex,
        const uint32_t                 queueIndex,
        const uint32_t                 dedicatedComputeUnits,
        const VkQueueGlobalPriorityKHR globalPriority,
        const uint32_t                 flags,
        size_t*                        palQueueMemorySize);

    static Pal::Result CreatePalQueues(
        const Device*                  pDevice,
        const uint32_t                 queueFamilyIndex,
        const uint32_t                 queueIndex,
        const uint32_t                 dedicatedComputeUnits,
        const VkQueueGlobalPriorityKHR globalPriority,
        const uint32_t                 flags,
        size_t*                        pPalQueueMemoryOffset,
        uint32_t*                      pDeviceIdx,
        void*                          pPalQueueMemory,
        Pal::IQueue**                  ppPalQueuesBase,
        Pal::IQueue**                  ppPalTmzQueues,
        Pal::IQueueSemaphore**         ppPalTmzSemaphores,
        Pal::IQueue**                  ppPalBackupQueues,
        Pal::IQueue**                  ppPalBackupTmzQueues,
        Pal::IQueueSemaphore**         ppSwitchToPalBackupSemaphore,
        Pal::IQueueSemaphore**         ppSwitchFromPalBackupSemaphore);

    ~Queue();

    void Destroy(
        Device*                      pDevice,
        const VkAllocationCallbacks* pAllocator);

    template<typename SubmitInfoType>
    VkResult Submit(
        uint32_t              submitCount,
        const SubmitInfoType* pSubmits,
        VkFence               fence);

    VkResult WaitIdle(void);
    VkResult PalSignalSemaphores(
        uint32_t            semaphoreCount,
        const VkSemaphore*  pSemaphores,
        const uint64_t*     pSemaphoreValues,
        const uint32_t      semaphoreDeviceIndicesCount,    // May be 0 to use DefaultDeviceIndex
        const uint32_t*     pSemaphoreDeviceIndices);

    VkResult PalWaitSemaphores(
        uint32_t            semaphoreCount,
        const VkSemaphore*  pSemaphores,
        const uint64_t*     pSemaphoreValues,
        const uint32_t      semaphoreDeviceIndicesCount,    // May be 0 to use DefaultDeviceIndex
        const uint32_t*     pSemaphoreDeviceIndices);

    VkResult Present(
        const VkPresentInfoKHR* pPresentInfo);

    VkResult BindSparse(
        uint32_t                                    bindInfoCount,
        const VkBindSparseInfo*                     pBindInfo,
        VkFence                                     fence);

    void InsertDebugUtilsLabel(
        const VkDebugUtilsLabelEXT*                 pLabelInfo);

    VkResult CreateSqttState(
        void* pMemory);

    enum
    {
        MaxQueueFamilies   = Pal::QueueTypeCount,  // Maximum number of queue families
        MaxQueuesPerFamily = 8,                    // Maximum number of queues per family
        MaxMultiQueues     = 4,

        MaxSubQueuesInGroup = MaxQueueFamilies * MaxQueuesPerFamily  // Maximum number of queues per group
    };

    VK_FORCEINLINE Pal::IQueue* PalQueue(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalQueues[idx];
    }

    VK_FORCEINLINE Pal::IQueue* PalTmzQueue(uint32_t idx) const
    {
        VK_ASSERT(idx < MaxPalDevices);
        return m_pPalTmzQueues[idx];
    }

    VK_FORCEINLINE Pal::IQueueSemaphore* PalTmzSemaphore(uint32_t idx) const
    {
        VK_ASSERT(idx < MaxPalDevices);
        return m_pPalTmzSemaphore[idx];
    }

    VK_FORCEINLINE Device* VkDevice() const
        { return m_pDevice; }

    uint32_t GetFamilyIndex() const
        { return m_queueFamilyIndex; }

    uint32_t GetIndex() const
        { return m_queueIndex; }

    uint32_t GetFlags() const
        { return m_queueFlags; }

    const Pal::PerSourceFrameMetadataControl* GetFrameMetadataControl() const
        { return &m_palFrameMetadataControl; }

    SqttQueueState* GetSqttState()
        { return m_pSqttState; }

    bool IsDeviceIndependent() const
    {
        return m_isDeviceIndependent;
    }

    VkResult SubmitInternalCmdBuf(
        CmdBufferRing*             pCmdBufferRing,
        uint32_t                   deviceIdx,
        const Pal::CmdBufInfo&     cmdBufInfo,
        CmdBufState*               pCmdBufState);

    VkResult SynchronizeBackBuffer(
        Memory* pMemory,
        uint32_t deviceIdx);

protected:
    // This is a helper structure during a virtual remap (sparse bind) call to batch remaps into
    // as few calls as possible.
    struct VirtualRemapState
    {
        uint32_t                      maxRangeCount;
        uint32_t                      rangeCount;
        Pal::VirtualMemoryRemapRange* pRanges;
    };

    // Per-VidPnSource flip status
    struct VidPnSourceFlipStatus
    {
        Pal::FlipStatusFlags flipFlags;    ///< PAL flip status flag
        bool                 isValid;      ///< Is the flip status valid
        bool                 isFlipOwner;  ///< Is the surface being flipped to was created by current device
    };

    union FullscreenFrameMetadataFlags
    {
        struct
        {
            uint32_t frameBeginFlag            :  1;
            uint32_t frameEndFlag              :  1;
            uint32_t primaryHandle             :  1;
            uint32_t reserved                  : 29;
        };
        uint32_t u32All;
    };

    VkResult BindSparseEntry(
        const VkBindSparseInfo& bindInfo,
        uint32_t                resourceDeviceIndex,
        uint32_t                memoryDeviceIndex,
        VkDeviceSize            prtTileSize,
        VirtualRemapState*      pRemapState,
        bool                    noWait);

    VkResult AddVirtualRemapRange(
        uint32_t           resourceDeviceIndex,
        Pal::IGpuMemory*   pVirtualGpuMem,
        VkDeviceSize       virtualOffset,
        Pal::IGpuMemory*   pRealGpuMem,
        VkDeviceSize       realOffset,
        VkDeviceSize       size,
        VirtualRemapState* pRemapState,
        bool               noWait);

    VkResult CommitVirtualRemapRanges(
        uint32_t           deviceIndex,
        VirtualRemapState* pRemapState,
        bool               noWait);

    VkResult UpdateFlipStatus(
        const Pal::PresentSwapChainInfo* pPresentInfo,
        const SwapChain*                 pSwapChain);

    bool NeedPacePresent(
        Pal::PresentSwapChainInfo* pPresentInfo,
        SwapChain*                 pSwapChain,
        bool*                      pSyncFlip,
        bool*                      pPostFrameTimerSubmission);

    VkResult CreateDummyCmdBuffer();

    bool BuildPostProcessCommands(
        uint32_t                         deviceIdx,
        CmdBufState*                     pCmdBufState,
        const Pal::IImage*               pImage,
        const Pal::PresentSwapChainInfo* pPresentInfo,
        const SwapChain*                 pSwapChain);

    VkResult NotifyFlipMetadata(
        uint32_t                     deviceIdx,
        Pal::IQueue*                 pPresentQueue,
        CmdBufState*                 pCmdBufState,
        const Pal::IGpuMemory*       pGpuMemory,
        FullscreenFrameMetadataFlags flags,
        bool                         forceSubmit = false);

    VkResult NotifyFlipMetadataBeforePresent(
        uint32_t                         deviceIdx,
        Pal::IQueue*                     pPresentQueue,
        const Pal::PresentSwapChainInfo* pPresentInfo,
        CmdBufState*                     pCmdBufState,
        const Pal::IGpuMemory*           pGpuMemory,
        bool                             forceSubmit,
        bool                             skipFsfmFlags);

    VkResult NotifyFlipMetadataAfterPresent(
        uint32_t                         deviceIdx,
        const Pal::PresentSwapChainInfo* pPresentInfo);

    Pal::IQueue*                       m_pPalQueues[MaxPalDevices];
    Pal::IQueue*                       m_pPalBackupQueues[MaxPalDevices];
    Pal::IQueue*                       m_pPalBackupTmzQueues[MaxPalDevices];
    Pal::IQueueSemaphore*              m_pSwitchToPalBackupSemaphore[MaxPalDevices];
    Pal::IQueueSemaphore*              m_pSwitchFromPalBackupSemaphore[MaxPalDevices];
    Pal::IQueue*                       m_pPalTmzQueues[MaxPalDevices];
    Pal::IQueueSemaphore*              m_pPalTmzSemaphore[MaxPalDevices];
    bool                               m_tmzPerQueue;
    bool                               m_lastSubmissionProtected;
    Device* const                      m_pDevice;
    uint32_t                           m_queueFamilyIndex;   // This queue's family index
    uint32_t                           m_queueIndex;         // This queue's index within the node group
    uint32_t                           m_queueFlags;
    DevModeMgr*                        m_pDevModeMgr;
    VirtualStackAllocator*             m_pStackAllocator;
    VidPnSourceFlipStatus              m_flipStatus;
    Pal::PerSourceFrameMetadataControl m_palFrameMetadataControl;
    Pal::ICmdBuffer*                   m_pDummyCmdBuffer[MaxPalDevices];
    SqttQueueState*                    m_pSqttState; // Per-queue state for handling SQ thread-tracing annotations
    CmdBufferRing*                     m_pCmdBufferRing;

    const bool                         m_isDeviceIndependent;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

VK_DEFINE_DISPATCHABLE(Queue);

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(
    VkQueue                                     queue);

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence);

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo2KHR*                     pSubmits,
    VkFence                                     fence);

VKAPI_ATTR VkResult VKAPI_CALL vkQueueBindSparse(
    VkQueue                                     queue,
    uint32_t                                    bindInfoCount,
    const VkBindSparseInfo*                     pBindInfo,
    VkFence                                     fence);

VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(
    VkQueue                                      queue,
    const VkPresentInfoKHR*                      pPresentInfo);

VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pLabelInfo);

VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(
    VkQueue                                     queue);

VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pLabelInfo);

};

}

#endif /* __VK_QUEUE_H__ */
