/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_device.h"

// PAL headers
#include "palHashMap.h"
#include "palQueue.h"
#include "palUtil.h"
#include "palList.h"
#include "palVector.h"

// gpuutil headers
#include "gpuUtil/palGpaSession.h"
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

// GPUOpen forward declarations
namespace DevDriver
{
class DevDriverServer;
class PipelineUriService;
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

namespace RGPProtocol
{
class RGPServer;
}
}

// Vulkan forward declarations
namespace vk
{
class Instance;
class Queue;
class Pipeline;
#if VKI_RAY_TRACING
class RayTracingPipeline;
#endif
class CmdBuffer;
class PipelineBinaryCache;
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

    // Pipeline hash used for instruction tracing whenever no pipeline is being targetted.
    static constexpr uint64_t InvalidTargetPipelineHash = 0;

    ~DevModeMgr();

    static VkResult Create(Instance* pInstance, DevModeMgr** ppObject);

    void Finalize(
        uint32_t              deviceCount,
        VulkanSettingsLoader* settingsLoaders[]);

    void Destroy();

    enum class FrameDelimiterType : uint32_t
    {
        QueuePresent,
        QueueLabel,
        CmdBufferTag,
        Count
    };

    void NotifyFrameBegin(const Queue* pQueue, FrameDelimiterType delimiterType);
    void NotifyFrameEnd(const Queue* pQueue, FrameDelimiterType delimiterType);
    void WaitForDriverResume();
    void PipelineCreated(Device* pDevice, Pipeline* pPipeline);
    void PipelineDestroyed(Device* pDevice, Pipeline* pPipeline);
#if VKI_RAY_TRACING
    void ShaderLibrariesCreated(Device* pDevice, RayTracingPipeline* pPipeline);
    void ShaderLibrariesDestroyed(Device* pDevice, RayTracingPipeline* pPipeline);
#endif
    void PostDeviceCreate(Device* pDevice);
    void PreDeviceDestroy(Device* pDevice);
    void NotifyPreSubmit();

    uint64_t GetInstructionTraceTargetHash();
    void StartInstructionTrace(CmdBuffer* pCmdBuffer);
    void StopInstructionTrace(CmdBuffer* pCmdBuffer);

    bool IsTracingEnabled() const;
    bool IsCrashAnalysisEnabled() const { return m_crashAnalysisEnabled; }

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
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore);

    Pal::Result TimedWaitQueueSemaphore(
        uint32_t                       deviceIdx,
        Queue*                         pQueue,
        VkSemaphore                    semaphore,
        uint64_t                       value,
        Pal::IQueueSemaphore*          pQueueSemaphore);

    inline bool IsQueueTimingActive(const Device* pDevice) const;
    inline bool GetTraceFrameBeginTag(uint64_t* pTag) const;
    inline bool GetTraceFrameEndTag(uint64_t* pTag) const;

    Util::Result RegisterPipelineCache(
        PipelineBinaryCache* pPipelineCache,
        uint32_t             postSizeLimit);

    void DeregisterPipelineCache(
        PipelineBinaryCache* pPipelineCache);
    Util::ListIterator<PipelineBinaryCache*, PalAllocator> GetPipelineCacheListIterator()
        { return m_pipelineCaches.Begin(); }

    Util::RWLock* GetPipelineReinjectionLock()
        { return &m_pipelineReinjectionLock; }

private:
    // Steps that an RGP trace goes through
    enum class TraceStatus : uint32_t
    {
        // "Pre-trace" stages:
        Idle = 0,           // No active trace and none requested
        Pending,            // We've identified that a trace has been requested and we've received its parameters,
                            // but we have not yet seen the first frame.
        Preparing,          // A trace has been requested but is not active yet because we are
                            // currently sampling timing information over some number of lead frames.
        Running,            // SQTT and queue timing is currently active for all command buffer submits.

        // "Post-trace" stages:
        WaitingForSqtt,     // Command to turn off SQTT has been submitted and we're waiting for fence confirmation.
        Ending              // Tracing is no longer active, but all results are not yet ready.
    };

    // Various trigger modes supported for RGP traces
    enum class TriggerMode : uint32_t
    {
        Present = 0, // Traces triggered by presents
        Index,       // Traces triggered by frame indices
        Tag          // Traces triggered by command buffer tags
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
        const Queue*           pQueue;
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
        bool                  labelDelimsPresent; // True is a label delimiter is recieved

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
        uint32_t              auxQueueCount;
        TraceQueueState       auxQueueStates[MaxTraceQueues]; // Used for queues belonging to other logical devices
                                                              // pointing to the same physical device
        uint32_t              queueFamilyCount;
        TraceQueueFamilyState queueFamilyState[MaxTraceQueueFamilies];

        uint32_t              activeCmdBufCount;   // Number of command buffers in below list
        Pal::ICmdBuffer*      pActiveCmdBufs[4];   // List of command buffers that need to be reset at end of trace
        uint32_t              preparedFrameCount;  // Number of frames counted while preparing for a trace
        uint32_t              sqttFrameCount;      // Number of frames counted while SQTT tracing is active
        uint64_t              frameBeginTag;       // If a command buffer with this debug-tag is submitted, it is
                                                   // treated as a virtual frame-start event.
        uint64_t              frameEndTag;         // Similarly to above but for frame-end post-submit.
    };

    DevModeMgr(Instance* pInstance);

    Pal::Result Init();

    void AdvanceActiveTraceStep(TraceState* pState, const Queue* pQueue, bool beginFrame, FrameDelimiterType delimiterType);
    void TraceIdleToPendingStep(TraceState* pState);
    Pal::Result TracePendingToPreparingStep(TraceState* pState, const Queue* pQueue, FrameDelimiterType delimiterType);
    Pal::Result TracePreparingToRunningStep(TraceState* pState, const Queue* pQueue);
    Pal::Result TraceRunningToWaitingForSqttStep(TraceState* pState, const Queue* pQueue);
    Pal::Result TraceWaitingForSqttToEndingStep(TraceState* pState, const Queue* pQueue);
    Pal::Result TraceEndingToIdleStep(TraceState* pState);
    void FinishOrAbortTrace(TraceState* pState, bool aborted);

    Pal::Result CheckTraceDeviceChanged(TraceState* pState, Device* pNewDevice);
    void DestroyRGPTracing(TraceState* pState);
    Pal::Result InitRGPTracing(TraceState* pState, Device* pDevice);
    Pal::Result InitTraceQueueResources(TraceState* pState, bool* pHasDebugVmid, const Queue* pQueue, bool auxQueue);
    Pal::Result InitTraceQueueResourcesForDevice(TraceState* pState, bool* pHasDebugVmid);
    Pal::Result InitTraceQueueFamilyResources(TraceState* pTraceState, TraceQueueFamilyState* pFamilyState);
    void DestroyTraceQueueFamilyResources(TraceQueueFamilyState* pState);
    TraceQueueState* FindTraceQueueState(TraceState* pState, const Queue* pQueue);
    bool QueueSupportsTiming(uint32_t deviceIdx, const Queue* pQueue);

    Instance*                           m_pInstance;
    DevDriver::DevDriverServer*         m_pDevDriverServer;
    DevDriver::RGPProtocol::RGPServer*  m_pRGPServer;
    DevDriver::PipelineUriService*      m_pPipelineUriService;
    Util::Mutex                         m_traceMutex;
    TraceState                          m_trace;
    bool                                m_finalized;
    TriggerMode                         m_triggerMode;              // Current trigger mode for RGP frame trace
    uint32_t                            m_numPrepFrames;
    uint32_t                            m_traceGpuMemLimit;
    bool                                m_enableInstTracing;        // Enable instruction-level SQTT tokens
    bool                                m_enableSampleUpdates;
    bool                                m_allowComputePresents;
    bool                                m_blockingTraceEnd;         // Wait on trace-end fences immediately.
    uint32_t                            m_globalFrameIndex;
    uint64_t                            m_traceFrameBeginTag;
    uint64_t                            m_traceFrameEndTag;
    uint32_t                            m_traceFrameBeginIndex;
    uint32_t                            m_traceFrameEndIndex;
    uint64_t                            m_targetApiPsoHash;
    uint32_t                            m_seMask;                   // Shader engine mask
    bool                                m_perfCountersEnabled;      // True if perf counters are enabled
    uint64_t                            m_perfCounterMemLimit;      // Memory limit for perf counters
    uint32_t                            m_perfCounterFrequency;     // Counter sample frequency
    bool                                m_useStaticVmid;
    bool                                m_staticVmidActive;
    bool                                m_crashAnalysisEnabled;

    using PerfCounterList = Util::Vector<GpuUtil::PerfCounterId, 8, PalAllocator>;

    PerfCounterList                     m_perfCounterIds;           // List of perf counter ids

    using PipelineCacheList = Util::List<PipelineBinaryCache*, PalAllocator>;

    PipelineCacheList                   m_pipelineCaches;
    Util::RWLock                        m_pipelineReinjectionLock;

    PAL_DISALLOW_DEFAULT_CTOR(DevModeMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(DevModeMgr);
#endif
};

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// Returns true if queue operations are currently being timed by RGP traces.
inline bool DevModeMgr::IsQueueTimingActive(
    const Device* pDevice
    ) const
{
    return (m_trace.queueTimingEnabled &&
            (m_trace.status == TraceStatus::Running ||
             m_trace.status == TraceStatus::Preparing ||
             m_trace.status == TraceStatus::WaitingForSqtt) &&
            (pDevice->VkPhysicalDevice(DefaultDeviceIndex) == m_trace.pDevice->VkPhysicalDevice(DefaultDeviceIndex)));
}

// =====================================================================================================================
bool DevModeMgr::GetTraceFrameBeginTag(
    uint64_t* pTag
    ) const
{
    bool active;

    if (m_trace.status != TraceStatus::Idle)
    {
        *pTag = m_traceFrameBeginTag;

        active = true;
    }
    else
    {
        active = false;
    }

    return active;
}

// =====================================================================================================================
bool DevModeMgr::GetTraceFrameEndTag(
    uint64_t*      pTag
    ) const
{
    bool active;

    if (m_trace.status != TraceStatus::Idle)
    {
        *pTag = m_traceFrameEndTag;

        active = true;
    }
    else
    {
        active = false;
    }

    return active;
}

#endif
};

#endif /* __DEVMODE_DEVMODE_MGR_H__ */
