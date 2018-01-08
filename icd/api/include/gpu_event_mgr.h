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
 **************************************************************************************************
 * @file  gpu_event_mgr.h
 * @brief Manages internal GPU events while building command buffers
 **************************************************************************************************
 */

#ifndef __GPU_EVENT_MGR_H__
#define __GPU_EVENT_MGR_H__

#pragma once

#include "include/khronos/vulkan.h"

#include "include/internal_mem_mgr.h"

#include "palIntrusiveList.h"

// Forward declare PAL classes used in this file
namespace Pal
{
struct CmdBufferBuildInfo;
class IGpuEvent;
};

// Forward declare Vulkan classes used in this file
namespace vk
{
class CmdBuffer;
class Device;
};

namespace vk
{

// =====================================================================================================================
// Class contains Pal::IGpuEvent* objects which are part of a device group
class GpuEvents
{

public:
    GpuEvents(uint32_t numDeviceEvents,
              Pal::IGpuEvent** pPalEvents) :
        m_numDeviceEvents(numDeviceEvents)
    {
        memcpy(m_pEvents, pPalEvents, sizeof(m_pEvents[0]) * numDeviceEvents);
    }

    void Destroy();

    VK_INLINE Pal::IGpuEvent* PalEvent(uint32_t deviceIdx) const
    {
        VK_ASSERT(deviceIdx < m_numDeviceEvents);
        return m_pEvents[deviceIdx];
    }

private:
    uint32_t        m_numDeviceEvents;
    Pal::IGpuEvent* m_pEvents[MaxPalDevices];
};

// =====================================================================================================================
// Manages GPU events used internally by command buffers.
class GpuEventMgr
{
public:
    typedef Util::IntrusiveList<GpuEventMgr> List;

    GpuEventMgr(Device* pDevice);
    ~GpuEventMgr();

    void     BeginCmdBuf(CmdBuffer* pOwner, const Pal::CmdBufferBuildInfo& info);
    VkResult RequestEvents(CmdBuffer* pCmdBuf, uint32_t eventCount, GpuEvents*** pppGpuEvents);
    void     ResetCmdBuf(CmdBuffer* pOwner);
    void     ResetEvents();
    void     Destroy();

    List::Node* ListNode() { return &m_parentNode; }

protected:
    struct EventChunk
    {
        EventChunk();

        InternalMemory  gpuMemory;
        GpuEvents**     ppGpuEvents;
        uint32_t        eventCount;
        uint32_t        eventNextFree;
        EventChunk*     pNextChunk;
    };

    void        DestroyChunk(EventChunk* pChunk);
    EventChunk* FindFreeExistingChunk(uint32_t eventCount);
    VkResult    CreateNewChunk(uint32_t eventCount, EventChunk** ppChunk);
    EventChunk* CreateChunkState(uint32_t eventCount);
    void        AllocEventsFromChunk(
                            CmdBuffer* pCmdBuf,
                            uint32_t eventCount,
                            EventChunk* pChunk,
                            GpuEvents*** ppGpuEvents);
    void        WaitToRecycleEvents(CmdBuffer* pCmdBuf);

    List::Node    m_parentNode;             // Intrusive list parent node
    EventChunk*   m_pFirstChunk;            // Linked list of event chunks
    bool          m_needWaitRecycleEvents;  // True if we still need to wait for previous access to events to complete
    Device* const m_pDevice;                // Device pointer
    uint32_t      m_totalEventCount;        // Total number of GPU event objects created so far
};

};

#endif /* __GPU_EVENT_MGR_H__ */
