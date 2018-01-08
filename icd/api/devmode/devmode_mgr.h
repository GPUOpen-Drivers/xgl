/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  devmode_mgr.h
* @brief Contains the GPU Open Developer Mode manager (DevModeMgr)
***********************************************************************************************************************
*/

#ifndef __DEVMODE_DEVMODE_MGR_H__
#define __DEVMODE_DEVMODE_MGR_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_queue.h"

// PAL headers
#include "palHashMap.h"
#include "palQueue.h"
#include "palUtil.h"

#if ICD_GPUOPEN_DEVMODE_BUILD
// gpuopen headers
#include "gpuopen.h"
#endif

// PAL forward declarations
namespace Pal
{
class  ICmdBuffer;
class  IFence;
class  IQueueSemaphore;
struct PalPublicSettings;
}

// GpuUtil forward declarations
namespace GpuUtil
{
class GpaSession;
};

// GPUOpen forward declarations
namespace DevDriver
{
class DevDriverServer;
class IMsgChannel;
struct MessageBuffer;

namespace DriverControlProtocol
{
enum struct DeviceClockMode : uint32_t;
class HandlerServer;
}

namespace SettingsProtocol
{
class HandlerServer;
struct Setting;
}

}

// Vulkan forward declarations
namespace vk
{
class Device;
class Instance;
class PhysicalDevice;
class Pipeline;
class Queue;
class SqttCmdBufferState;
};

namespace vk
{

// =====================================================================================================================
// This class provides functionality to interact with the GPU Open Developer Mode message passing service and the rest
// of the driver.
class DevModeMgr
{
#if ICD_GPUOPEN_DEVMODE_BUILD
public:
    // Number of frames to wait before collecting a hardware trace.
    // Note: This will be replaced in the future by a remotely configurable value provided by the RGP server.
    static constexpr uint32_t NumTracePreparationFrames = 4;

    ~DevModeMgr();

    static VkResult Create(Instance* pInstance, DevModeMgr** ppObject);

    void Finalize(
        uint32_t         deviceCount,
        Pal::IDevice**   ppDevices,
        RuntimeSettings* pSettings);

    void Destroy();

    void PrePresent(const Queue* pQueue);
    void PostPresent(const Queue* pQueue);
    void WaitForDriverResume();
    void PipelineCreated(Device* pDevice, Pipeline* pPipeline);

    VK_INLINE bool IsTracingEnabled() const
        { VK_ASSERT(m_finalized); return m_tracingEnabled; }

    void PostDeviceCreate(Device* pDevice);
    void PreDeviceDestroy(Device* pDevice);

    Pal::Result TimedQueueSubmit(
        uint32_t               deviceIdx,
        Queue*                 pQueue,
        uint32_t               cmdBufferCount,
        const VkCommandBuffer* pCommandBuffers,
        const Pal::SubmitInfo& submitInfo,
        VirtualStackFrame*     pVirtStackFrame);

    Pal::Result TimedSignalQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        Pal::IQueueSemaphore*          pQueueSemaphore);

    Pal::Result TimedWaitQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        Pal::IQueueSemaphore*          pQueueSemaphore);

    VK_INLINE bool IsQueueTimingActive(const Device* pDevice) const;

private:
    // Steps that an RGP trace goes through
    enum class TraceStatus
    {
        Idle = 0,           // No active trace and none requested
        Preparing,          // A trace has been requested but is not active yet because we are
                            // currently sampling timing information over some number of lead frames.
        Running,            // SQTT and queue timing is currently active for all command buffer submits.
        WaitingForSqtt,
        WaitingForResults   // Tracing is no longer active, but all results are not yet ready.
    };

    // Queue family (type)-specific state to support RGP tracing (part of device state)
    struct TraceQueueFamilyState
    {
        uint32_t         queueFamilyIndex;
        Pal::QueueType   queueType;
        Pal::EngineType  engineType;
        Pal::ICmdBuffer* pTraceBeginCmdBuf;
        Pal::ICmdBuffer* pTraceBeginSqttCmdBuf;
        Pal::ICmdBuffer* pTraceEndSqttCmdBuf;
        Pal::ICmdBuffer* pTraceEndCmdBuf;
        Pal::ICmdBuffer* pTraceFlushCmdBuf;
        bool             supportsTracing;
        bool             usedForBegin;
        bool             usedForEndSqtt;
        bool             usedForEnd;
    };

    // Queue-specific resources to support RGP tracing (part of device state)
    struct TraceQueueState
    {
        Queue*                 pQueue;
        TraceQueueFamilyState* pFamily;
        Pal::uint64            queueId;
        Pal::uint64            queueContext;
        bool                   timingSupported;
    };

    static constexpr uint32_t MaxTraceQueueFamilies = Queue::MaxQueueFamilies;
    static constexpr uint32_t MaxTraceQueues        = MaxTraceQueueFamilies * Queue::MaxQueuesPerFamily;

    // All per-device state to support RGP tracing
    struct TraceState
    {
        TraceStatus           status;             // Current trace status (idle, running, etc.)

        Device*               pDevice;            // The device currently doing the tracing
        Pal::ICmdAllocator*   pCmdAllocator;      // Command allocator for creating trace-begin/end buffers
        Pal::IFence*          pBeginFence;        // Fence that is signaled when a trace-begin cmdbuf retires
        Pal::IFence*          pEndSqttFence;      // Fence that is signaled when a trace-end cmdbuf retires
        Pal::IFence*          pEndFence;          // Fence that is signaled when a trace-end cmdbuf retires
        TraceQueueState*      pTracePrepareQueue; // The queue that triggered the full start of a trace
        TraceQueueState*      pTraceBeginQueue;   // The queue that triggered starting SQTT
        TraceQueueState*      pTraceEndSqttQueue; // The queue that triggered ending SQTT
        TraceQueueState*      pTraceEndQueue;     // The queue that triggered the full end of a trace

        GpuUtil::GpaSession*  pGpaSession;        // GPA session helper object for building RGP data
        uint32_t              gpaSampleId;        // Sample ID associated with the current trace
        bool                  queueTimingEnabled; // Queue timing is enabled
        bool                  flushAllQueues;     // Flushes all queues during the last preparation frame.

        // Queue-specific state/information for tracing:
        uint32_t              queueCount;
        TraceQueueState       queueState[MaxTraceQueues];
        uint32_t              queueFamilyCount;
        TraceQueueFamilyState queueFamilyState[MaxTraceQueueFamilies];

        uint32_t              activeCmdBufCount;   // Number of command buffers in below list
        Pal::ICmdBuffer*      pActiveCmdBufs[4];   // List of command buffers that need to be reset at end of trace
        uint32_t              preparedFrameCount;  // Number of frames counted while preparing for a trace
        uint32_t              sqttFrameCount;      // Number of frames counted while SQTT tracing is active
    };

    DevModeMgr(Instance* pInstance);

    Pal::Result Init();
    Pal::Result PrepareRGPTrace(TraceState* pState, const Queue* pQueue);
    Pal::Result BeginRGPTrace(TraceState* pState, const Queue* pQueue);
    Pal::Result EndRGPHardwareTrace(TraceState* pState, const Queue* pQueue);
    Pal::Result EndRGPTrace(TraceState* pState, const Queue* pQueue);
    void FinishRGPTrace(TraceState* pState, bool aborted);
    Pal::Result CheckTraceDeviceChanged(TraceState* pState, Device* pNewDevice);
    void DestroyRGPTracing(TraceState* pState);
    Pal::Result InitRGPTracing(TraceState* pState, Device* pDevice);
    Pal::Result InitTraceQueueResources(TraceState* pState, bool* pHasDebugVmid);
    Pal::Result InitTraceQueueFamilyResources(TraceState* pTraceState, TraceQueueFamilyState* pFamilyState);
    void DestroyTraceQueueFamilyResources(TraceQueueFamilyState* pState);
    TraceQueueState* FindTraceQueueState(TraceState* pState, const Queue* pQueue);
    Pal::Result CheckForTraceResults(TraceState* pState);
    bool QueueSupportsTiming(uint32_t deviceIdx, const Queue* pQueue);
    static bool GpuSupportsTracing(const Pal::DeviceProperties& props, const RuntimeSettings& settings);

    Instance*                   m_pInstance;
    DevDriver::DevDriverServer* m_pDevDriverServer;
    Util::Mutex                 m_traceMutex;
    TraceState                  m_trace;
    bool                        m_hardwareSupportsTracing;  // True if gfxip supports tracing
    bool                        m_rgpServerSupportsTracing; // True if gpuopen protocol successfully enabled tracing
    bool                        m_finalized;
    bool                        m_tracingEnabled;           // True if tracing is currently enabled (master flag)
    uint32_t                    m_numPrepFrames;
    uint32_t                    m_traceGpuMemLimit;
    bool                        m_enableInstTracing;        // Enable instruction-level SQTT tokens
    bool                        m_enableSampleUpdates;
    uint32_t                    m_globalFrameIndex;

    PAL_DISALLOW_DEFAULT_CTOR(DevModeMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(DevModeMgr);
#endif
};

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// Returns true if queue operations are currently being timed by RGP traces.
VK_INLINE bool DevModeMgr::IsQueueTimingActive(
    const Device* pDevice
    ) const
{
    return (m_trace.queueTimingEnabled &&
            (m_trace.status == TraceStatus::Running ||
             m_trace.status == TraceStatus::Preparing ||
             m_trace.status == TraceStatus::WaitingForSqtt) &&
            (pDevice == m_trace.pDevice));
}
#endif
};

#endif /* __DEVMODE_DEVMODE_MGR_H__ */
