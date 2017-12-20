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

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"

#include "palGpuEvent.h"
#include "palIntrusiveListImpl.h"

namespace vk
{

// =====================================================================================================================
void GpuEvents::Destroy()
{
    for (uint32_t i = 0; i < m_numDeviceEvents; i++)
    {
        m_pEvents[i]->Destroy();
    }
}

// =====================================================================================================================
GpuEventMgr::GpuEventMgr(Device* pDevice)
    :
    m_parentNode(this),
    m_pFirstChunk(nullptr),
    m_needWaitRecycleEvents(false),
    m_pDevice(pDevice),
    m_totalEventCount(0)
{

}

// =====================================================================================================================
GpuEventMgr::~GpuEventMgr()
{
    Destroy();
}

// =====================================================================================================================
// Should be called during the parent's vkBeginCommandBuffer()
void GpuEventMgr::BeginCmdBuf(
    CmdBuffer*                     pOwner,
    const Pal::CmdBufferBuildInfo& info)
{
    // If this command buffer can be submitted multiple times, we need to make sure that we wait on its previous
    // incarnation to complete before allowing any events to be accessed.  This is because we need to make sure nothing
    // signals these events while the GPU is still accessing this command buffer.
    m_needWaitRecycleEvents = (info.flags.optimizeOneTimeSubmit == false);
}

// =====================================================================================================================
// Called when this event manager's event memory should be reset.  This will mark all events as free for allocation but
// does not release any of their GPU memory.
//
// This is called either when a command buffer is being reset, or when a command buffer's resources are being
// are being released back to the command pool (e.g. when destroyed).
void GpuEventMgr::ResetEvents()
{
    // Mark all previously-created events as free for reuse.  When resetting a command buffer, the application is
    // responsible for ensuring that no previous access to the command buffer by the GPU is pending which means that
    // we don't need to wait before resetting the GPU value of these events (this actual reset happens during
    // RequestEvents()).
    EventChunk* pChunk = m_pFirstChunk;

    while (pChunk != nullptr)
    {
        pChunk->eventNextFree = 0;
        pChunk = pChunk->pNextChunk;
    }
}

// =====================================================================================================================
// Called when the command buffer that owns this event manager is reset.
void GpuEventMgr::ResetCmdBuf(
    CmdBuffer* pOwner)
{
    // Reset all events back to available.
    ResetEvents();
}

// =====================================================================================================================
// Destroys the event manager's internal memory
void GpuEventMgr::Destroy()
{
    Instance* pInstance = m_pDevice->VkInstance();

    EventChunk* pChunk = m_pFirstChunk;

    while (pChunk != nullptr)
    {
        EventChunk* pNext = pChunk->pNextChunk;

        DestroyChunk(pChunk);

        pChunk = pNext;
    }

    m_pFirstChunk = nullptr;
    m_totalEventCount = 0;
}

// =====================================================================================================================
// Destroys the given batch of GPU events.  Called when the command buffer is destroyed or as part of allocation
// failure clean-up.
void GpuEventMgr::DestroyChunk(EventChunk* pChunk)
{
    if (pChunk != nullptr)
    {
        for (uint32_t i = 0; i < pChunk->eventCount; ++i)
        {
            pChunk->ppGpuEvents[i]->Destroy();
        }

        m_pDevice->MemMgr()->FreeGpuMem(&pChunk->gpuMemory);

        m_pDevice->VkInstance()->FreeMem(pChunk);
    }
}

// =====================================================================================================================
// Requests some number of events to be given to the command buffer.
//
// WARNING: THIS FUNCTIONALITY IS INCOMPATIBLE WITH COMMAND BUFFERS THAT CAN BE SUBMITTED IN PARALLEL ON MULTIPLE
// QUEUES.  PARALLEL EXECUTION OF THE SAME COMMAND BUFFER WILL CAUSE IT TO TRIP OVER ITS OWN EVENTS.
//
// There is currently no use case for that with the exception of compute engine command buffers and such command
// buffers should not make use of this functionality.
VkResult GpuEventMgr::RequestEvents(
    CmdBuffer*        pCmdBuf,
    uint32_t          eventCount,
    GpuEvents***      pppGpuEvents)
{
    if (eventCount == 0)
    {
        *pppGpuEvents = nullptr;

        return VK_SUCCESS;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    const Pal::DeviceProperties& deviceProps = m_pDevice->VkPhysicalDevice()->PalProperties();
    const Pal::EngineType engineType = pCmdBuf->GetPalEngineType();

    // See above comment
    VK_ASSERT(deviceProps.engineProperties[engineType].engineCount == 1);
#endif

    if (m_needWaitRecycleEvents)
    {
        WaitToRecycleEvents(pCmdBuf);
    }

    VkResult result = VK_SUCCESS;

    EventChunk* pChunk = FindFreeExistingChunk(eventCount);

    if (pChunk == nullptr)
    {
        result = CreateNewChunk(eventCount, &pChunk);
    }

    if (pChunk != nullptr)
    {
        VK_ASSERT(result == VK_SUCCESS);

        AllocEventsFromChunk(pCmdBuf, eventCount, pChunk, pppGpuEvents);
    }

    return result;
}

// =====================================================================================================================
// Tries to find enough space in an existing batch of GPU events.
GpuEventMgr::EventChunk* GpuEventMgr::FindFreeExistingChunk(uint32_t eventCount)
{
    EventChunk* pChunk = m_pFirstChunk;

    while (pChunk != nullptr)
    {
        if (pChunk->eventCount - pChunk->eventNextFree >= eventCount)
        {
            return pChunk;
        }

        pChunk = pChunk->pNextChunk;
    }

    return nullptr;
}

// =====================================================================================================================
// Allocates GPU events from the given chunk of events.
void GpuEventMgr::AllocEventsFromChunk(
    CmdBuffer*        pCmdBuf,
    uint32_t          eventCount,
    EventChunk*       pChunk,
    GpuEvents***      pppGpuEvents)
{
    GpuEvents** ppEvents = pChunk->ppGpuEvents + pChunk->eventNextFree;

    pChunk->eventNextFree += eventCount;

    VK_ASSERT(pChunk->eventNextFree <= pChunk->eventCount);

    // Reset the event status
    // Note that the top of pipe reset below is okay because any previous reads have already been taken care of by the
    // insertion of the inter-submit barrier
    VK_ASSERT(m_needWaitRecycleEvents == false);

    for (uint32_t i = 0; i < eventCount; ++i)
    {
        pCmdBuf->PalCmdResetEvent(ppEvents[i], Pal::HwPipeTop);
    }

    *pppGpuEvents = ppEvents;
}

// =====================================================================================================================
// Creates a new chunk at least large enough to fit the requested number of events.
VkResult GpuEventMgr::CreateNewChunk(
    uint32_t     eventCount,
    EventChunk** ppChunk)
{
    const auto& settings = m_pDevice->VkPhysicalDevice()->GetRuntimeSettings();

    if (eventCount < settings.cmdBufGpuEventMinAllocCount)
    {
        eventCount = settings.cmdBufGpuEventMinAllocCount;
    }

    VkResult result = VK_SUCCESS;

    EventChunk* pChunk = CreateChunkState(eventCount);

    if (pChunk != nullptr)
    {
        pChunk->pNextChunk = m_pFirstChunk;
        m_pFirstChunk = pChunk;

        m_totalEventCount += pChunk->eventCount;

        *ppChunk = pChunk;
    }
    else
    {
        DestroyChunk(pChunk);

        *ppChunk = nullptr;
    }

    return result;
}

// =====================================================================================================================
GpuEventMgr::EventChunk::EventChunk()
    :
    ppGpuEvents(nullptr),
    eventCount(0),
    eventNextFree(0),
    pNextChunk(nullptr)
{

}

// =====================================================================================================================
// Initializes the system memory and state of a new event chunk.
GpuEventMgr::EventChunk* GpuEventMgr::CreateChunkState(uint32_t eventCount)
{
    size_t totalSize = 0;

    size_t chunkHeaderSize = sizeof(EventChunk);

    totalSize += chunkHeaderSize;

    size_t eventPtrArraySize = eventCount * sizeof(GpuEvents);

    totalSize += eventPtrArraySize;

    size_t eventPalObjSize = 0;
    Pal::GpuEventCreateInfo eventCreateInfo = {};
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        eventPalObjSize += m_pDevice->PalDevice(deviceIdx)->GetGpuEventSize(eventCreateInfo, nullptr);
    }

    size_t eventSysMemSize = eventCount * (sizeof(GpuEvents) + eventPalObjSize);

    totalSize += eventSysMemSize;

    void* pMem = m_pDevice->VkInstance()->AllocMem(totalSize, VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);
    void* pOrigMem = pMem;

    if (pMem == nullptr)
    {
        return nullptr;
    }

    EventChunk* pChunk = reinterpret_cast<EventChunk*>(pMem);
    pMem = Util::VoidPtrInc(pMem, chunkHeaderSize);

    VK_PLACEMENT_NEW(pChunk) GpuEventMgr::EventChunk();

    pChunk->ppGpuEvents = reinterpret_cast<GpuEvents**>(pMem);

    pMem = Util::VoidPtrInc(pMem, eventPtrArraySize);

    Pal::Result result = Pal::Result::Success;

    const Pal::GpuEventCreateInfo createInfo = {};

    for (pChunk->eventCount = 0;
         (pChunk->eventCount < eventCount) && (result == Pal::Result::Success);
         pChunk->eventCount++)
    {
        Pal::IGpuEvent* pPalEvents[MaxPalDevices] = {};

        size_t memOffset = sizeof(GpuEvents);
        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            result = m_pDevice->PalDevice(deviceIdx)->CreateGpuEvent(createInfo,
                                    Util::VoidPtrInc(pMem, memOffset), &pPalEvents[deviceIdx] );

            memOffset += m_pDevice->PalDevice(deviceIdx)->GetGpuEventSize(createInfo, nullptr);
        }
        VK_PLACEMENT_NEW(pMem) GpuEvents(m_pDevice->NumPalDevices(), pPalEvents);

        pChunk->ppGpuEvents[pChunk->eventCount] = reinterpret_cast<GpuEvents*>(pMem);

        pMem = Util::VoidPtrInc(pMem, sizeof(GpuEvents) + eventPalObjSize);
    }

    VK_ASSERT(Util::VoidPtrDiff(pMem, pOrigMem) == totalSize);

    if (result == Pal::Result::Success)
    {
        return pChunk;
    }
    else
    {
        return nullptr;
    }
}

// =====================================================================================================================
// Waits for any previous access to all events to finish.
void GpuEventMgr::WaitToRecycleEvents(CmdBuffer* pCmdBuf)
{
    Pal::BarrierInfo barrier = {};
    Pal::HwPipePoint signalPoint = Pal::HwPipeTop;

    barrier.flags.u32All          = 0;
    barrier.waitPoint             = Pal::HwPipeTop;
    barrier.pipePointWaitCount    = 1;
    barrier.pPipePoints           = &signalPoint;
    barrier.pSplitBarrierGpuEvent = nullptr;

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        pCmdBuf->PalCmdBuffer(deviceIdx)->CmdBarrier(barrier);
    }

    m_needWaitRecycleEvents = false;
}

};
