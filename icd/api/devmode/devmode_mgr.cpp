/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  devmode_mgr.cpp
 * @brief Contains implementation of the GPU Open Developer Mode manager
 ***********************************************************************************************************************
 */

#if ICD_GPUOPEN_DEVMODE_BUILD
// Vulkan headers
#include "devmode/devmode_mgr.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline.h"
#include "include/vk_physical_device.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

// PAL headers
#include "palCmdAllocator.h"
#include "palFence.h"
#include "palQueueSemaphore.h"

// gpuutil headers
#include "gpuUtil/palGpaSession.h"

// gpuopen headers
#include "devDriverServer.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/rgpServer.h"
#include "protocols/driverControlServer.h"

#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
#include "protocols/etwClient.h"
#endif

namespace vk
{

constexpr uint64_t InfiniteTimeout = static_cast<uint64_t>(1e10);

// =====================================================================================================================
// Translates a DevDriver result to a VkResult.
static VkResult DevDriverToVkResult(
    DevDriver::Result devResult)
{
    VkResult result;

    switch (devResult)
    {
    case DevDriver::Result::Success:
        result = VK_SUCCESS;
        break;
    case DevDriver::Result::Error:
    case DevDriver::Result::Unavailable:
        result = VK_ERROR_INITIALIZATION_FAILED;
        break;
    case DevDriver::Result::NotReady:
        result = VK_NOT_READY;
        break;
    default:
        VK_NEVER_CALLED();
        result = VK_ERROR_INITIALIZATION_FAILED;
        break;
    }

    return result;
}

// =====================================================================================================================
// Translates a DevDriver result to a Pal::Result.
static Pal::Result DevDriverToPalResult(
    DevDriver::Result devResult)
{
    Pal::Result result;

    switch (devResult)
    {
    case DevDriver::Result::Success:
        result = Pal::Result::Success;
        break;
    case DevDriver::Result::Error:
        result = Pal::Result::ErrorUnknown;
        break;
    case DevDriver::Result::Unavailable:
        result = Pal::Result::ErrorUnavailable;
        break;
    case DevDriver::Result::NotReady:
        result = Pal::Result::NotReady;
        break;
    default:
        VK_NEVER_CALLED();
        result = Pal::Result::ErrorInitializationFailed;
        break;
    }

    return result;
}

// =====================================================================================================================
DevModeMgr::DevModeMgr(Instance* pInstance)
    :
    m_pInstance(pInstance),
    m_pDevDriverServer(pInstance->PalPlatform()->GetDevDriverServer()),
    m_pRGPServer(nullptr),
    m_pPipelineUriService(nullptr),
    m_pipelineServiceRegistered(false),
#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
    m_pEtwClient(nullptr),
#endif
    m_hardwareSupportsTracing(false),
    m_rgpServerSupportsTracing(false),
    m_finalized(false),
    m_tracingEnabled(false),
    m_numPrepFrames(0),
    m_traceGpuMemLimit(0),
    m_enableInstTracing(false),
    m_enableSampleUpdates(false),
    m_allowComputePresents(false),
    m_blockingTraceEnd(false),
    m_globalFrameIndex(1), // Must start from 1 according to RGP spec
    m_traceFrameBeginTag(0),
    m_traceFrameEndTag(0)
{
    memset(&m_trace, 0, sizeof(m_trace));
}

// =====================================================================================================================
DevModeMgr::~DevModeMgr()
{
    DestroyRGPTracing(&m_trace);
}

// =====================================================================================================================
// Creates the GPU Open Developer Mode manager class.
VkResult DevModeMgr::Create(
    Instance*              pInstance,
    DevModeMgr**           ppObject)
{
    Pal::Result result = Pal::Result::Success;

    void* pStorage = pInstance->AllocMem(sizeof(DevModeMgr), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    if (pStorage != nullptr)
    {
        DevModeMgr* pMgr = VK_PLACEMENT_NEW(pStorage) DevModeMgr(pInstance);

        result = pMgr->Init();

        if (result == Pal::Result::Success)
        {
            *ppObject = pMgr;
        }
        else
        {
            pMgr->Destroy();
        }
    }
    else
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
// Initializes the devmode manager based on the current client flags.
Pal::Result DevModeMgr::Init()
{
    Pal::Result result = m_traceMutex.Init();

    if (m_pDevDriverServer != nullptr)
    {
        m_pRGPServer = m_pDevDriverServer->GetRGPServer();
    }

    // Tell RGP that the server (i.e. the driver) supports tracing if requested.
    if (result == Pal::Result::Success)
    {
        if (m_pRGPServer != nullptr)
        {
            m_rgpServerSupportsTracing = (m_pRGPServer->EnableTraces() == DevDriver::Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// Called during initial device enumeration prior to calling Pal::IDevice::CommitSettingsAndInit().
//
// This finalizes the developer driver manager.
void DevModeMgr::Finalize(
    uint32_t         deviceCount,
    Pal::IDevice**   ppDevices,
    RuntimeSettings* pSettings)
{
    // Figure out if the gfxip supports tracing.  We decide tracing if there is at least one enumerated GPU
    // that can support tracing.  Since we don't yet know if that GPU will be picked as the target of an eventual
    // VkDevice, this check is imperfect.  In mixed-GPU situations where an unsupported GPU is picked for tracing,
    // trace capture will fail with an error.
    m_hardwareSupportsTracing = false;

    if (m_rgpServerSupportsTracing)
    {
        for (uint32_t gpu = 0; gpu < deviceCount; ++gpu)
        {
            // This is technically a violation of the PAL interface: we are not allowed to query PAL device properties
            // prior to calling CommitSettingsAndInit().  However, doing so is (a) safe for some properties and (b)
            // the only way to do this currently as we need to know this information prior to calling Finalize() on
            // the device and devdriver manager and (c) it also matches DXCP behavior for the same reasons.
            Pal::DeviceProperties props = {};

            if (ppDevices[gpu]->GetProperties(&props) == Pal::Result::Success)
            {
                if (GpuSupportsTracing(props, pSettings[gpu]))
                {
                    m_hardwareSupportsTracing = true;

                    break;
                }
            }
        }
    }

    // If no GPU supports tracing, inform the RGP server to disable tracing
    if ((m_pRGPServer != nullptr) && (m_hardwareSupportsTracing == false))
    {
        m_pRGPServer->DisableTraces();
    }

    // Finalize the devmode manager
    m_pDevDriverServer->Finalize();

    // Figure out if tracing support should be enabled or not
    m_finalized      = true;
    m_tracingEnabled = (m_pRGPServer != nullptr) && m_pRGPServer->TracesEnabled();
}

// =====================================================================================================================
// Destroy the developer mode manager
void DevModeMgr::Destroy()
{
    Util::Destructor(this);

    m_pInstance->FreeMem(this);
}

// =====================================================================================================================
// Waits for the driver to be resumed if it's currently paused.
void DevModeMgr::WaitForDriverResume()
{
    auto* pDriverControlServer = m_pDevDriverServer->GetDriverControlServer();

    VK_ASSERT(pDriverControlServer != nullptr);

    pDriverControlServer->WaitForDriverResume();
}

// =====================================================================================================================
// Called to notify of a frame-end boundary and is used to coordinate RGP trace start/stop.
//
// If "actualPresent" is true, this call is coming from an actual present call.  Otherwise, it is a virtual
// frame end boundary.
void DevModeMgr::NotifyFrameEnd(
    const Queue* pQueue,
    bool         actualPresent)
{
    // Get the RGP message server
    if ((m_pRGPServer != nullptr) && m_pRGPServer->TracesEnabled())
    {
        // Only act if this present is coming from the same device that started the trace
        if (m_trace.status != TraceStatus::Idle)
        {
            Util::MutexAuto traceLock(&m_traceMutex);

            if (m_trace.status != TraceStatus::Idle)
            {
                if (IsQueueTimingActive(pQueue->VkDevice()))
                {
                    // Call TimedQueuePresent() to insert commands that collect GPU timestamp.
                    Pal::IQueue* pPalQueue = pQueue->PalQueue(DefaultDeviceIndex);

                    // Currently nothing in the PresentInfo struct is used for inserting a timed present marker.
                    GpuUtil::TimedQueuePresentInfo timedPresentInfo = {};

                    Pal::Result result = m_trace.pGpaSession->TimedQueuePresent(pPalQueue, timedPresentInfo);

                    VK_ASSERT(result == Pal::Result::Success);
                }

                // Increment trace frame counters.  These control when the trace can transition
                if (m_trace.status == TraceStatus::Preparing)
                {
                    m_trace.preparedFrameCount++;
                }
                else if (m_trace.status == TraceStatus::Running)
                {
                    m_trace.sqttFrameCount++;
                }

                AdvanceActiveTraceStep(&m_trace, pQueue, false, actualPresent);
            }
        }
    }

    m_globalFrameIndex++;
}

// =====================================================================================================================
void DevModeMgr::AdvanceActiveTraceStep(
    TraceState*  pState,
    const Queue* pQueue,
    bool         beginFrame,
    bool         actualPresent)
{
    VK_ASSERT(pState->status != TraceStatus::Idle);

    // Only advance the trace step if we're processing the right type of trigger.
    // In present trigger mode, we should only advance when a real present occurs.
    // In tag trigger mode, we should only advance when a tag trigger is encountered.
    // We only support two trigger modes, so any time this is called with actualPresent set to false, we can safely
    // assume this is being called because of a tag trigger.
    const bool isPresentTrigger = actualPresent;
    const bool isTagTrigger     = (actualPresent == false);
    const bool isValidTrigger   = ((m_trace.triggerMode == TriggerMode::Present) ? isPresentTrigger : isTagTrigger);

    if (isValidTrigger)
    {
        if (m_trace.status == TraceStatus::Pending)
        {
            // Attempt to start preparing for a trace
            if (TracePendingToPreparingStep(&m_trace, pQueue, actualPresent) != Pal::Result::Success)
            {
                FinishOrAbortTrace(&m_trace, true);
            }
        }

        if (m_trace.status == TraceStatus::Preparing)
        {
            if (TracePreparingToRunningStep(&m_trace, pQueue) != Pal::Result::Success)
            {
                FinishOrAbortTrace(&m_trace, true);
            }
        }

        if (m_trace.status == TraceStatus::Running)
        {
            if (TraceRunningToWaitingForSqttStep(&m_trace, pQueue) != Pal::Result::Success)
            {
                FinishOrAbortTrace(&m_trace, true);
            }
        }

        if (m_trace.status == TraceStatus::WaitingForSqtt)
        {
            if (TraceWaitingForSqttToEndingStep(&m_trace, pQueue) != Pal::Result::Success)
            {
                FinishOrAbortTrace(&m_trace, true);
            }
        }

        if (m_trace.status == TraceStatus::Ending)
        {
            Pal::Result result = TraceEndingToIdleStep(&m_trace);

            // Results ready: finish trace
            if (result == Pal::Result::Success)
            {
                FinishOrAbortTrace(&m_trace, false);
            }
            // Error while computing results: abort trace
            else if (result != Pal::Result::NotReady)
            {
                FinishOrAbortTrace(&m_trace, true);
            }
        }
    }
}

// =====================================================================================================================
// Checks if all trace results are ready and finalizes the results, transmitting data through gpuopen.
//
// Transitions from Ending to Idle step.
Pal::Result DevModeMgr::TraceEndingToIdleStep(TraceState* pState)
{
    VK_ASSERT(pState->status == TraceStatus::Ending);

    Pal::Result result = Pal::Result::NotReady;

    if (m_blockingTraceEnd)
    {
        result = pState->pDevice->PalDevice(DefaultDeviceIndex)->WaitForFences(1, &pState->pEndFence, true, InfiniteTimeout);

        if (result != Pal::Result::Success)
        {
            return result;
        }

        while (pState->pGpaSession->IsReady() == false)
        {
            Util::YieldThread();
        }
    }

    // Check if trace results are ready
    if (pState->pGpaSession->IsReady()                              && // GPA session is ready
        (pState->pBeginFence->GetStatus() != Pal::Result::NotReady) && // "Trace begin" cmdbuf has retired
        (pState->pEndFence->GetStatus()   != Pal::Result::NotReady))   // "Trace end" cmdbuf has retired
    {
#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
        // Process ETW events if we have a connected client.
        if (m_pEtwClient != nullptr)
        {
            // Disable tracing on the ETW client.
            size_t numGpuEvents = 0;
            DevDriver::Result devDriverResult = m_pEtwClient->DisableTracing(&numGpuEvents);

            // Inject any external signal and wait events if we have any.
            if ((devDriverResult == DevDriver::Result::Success) && (numGpuEvents > 0))
            {
                // Allocate memory for the gpu events.
                DevDriver::GpuEvent* pGpuEvents = reinterpret_cast<DevDriver::GpuEvent*>(m_pInstance->AllocMem(
                    sizeof(DevDriver::GpuEvent) * numGpuEvents,
                    VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE));
                if (pGpuEvents == nullptr)
                {
                    devDriverResult = DevDriver::Result::InsufficientMemory;
                }
                else if (devDriverResult == DevDriver::Result::Success)
                {
                    devDriverResult = m_pEtwClient->GetTraceData(pGpuEvents, numGpuEvents);
                }

                if (devDriverResult == DevDriver::Result::Success)
                {
                    for (uint32_t eventIndex = 0; eventIndex < static_cast<uint32_t>(numGpuEvents); ++eventIndex)
                    {
                        Pal::Result palResult = Pal::Result::Success;

                        const DevDriver::GpuEvent* pGpuEvent = &pGpuEvents[eventIndex];
                        if (pGpuEvent->type == DevDriver::GpuEventType::QueueSignal)
                        {
                            GpuUtil::TimedQueueSemaphoreInfo signalInfo = {};
                            signalInfo.semaphoreID = pGpuEvent->queue.fenceObject;

                            palResult =
                                pState->pGpaSession->ExternalTimedSignalQueueSemaphore(
                                    pGpuEvent->queue.contextIdentifier,
                                    pGpuEvent->submissionTime,
                                    pGpuEvent->completionTime,
                                    signalInfo);
                        }
                        else if (pGpuEvent->type == DevDriver::GpuEventType::QueueWait)
                        {
                            GpuUtil::TimedQueueSemaphoreInfo waitInfo = {};
                            waitInfo.semaphoreID = pGpuEvent->queue.fenceObject;

                            palResult =
                                pState->pGpaSession->ExternalTimedWaitQueueSemaphore(
                                    pGpuEvent->queue.contextIdentifier,
                                    pGpuEvent->submissionTime,
                                    pGpuEvent->completionTime,
                                    waitInfo);
                        }

                        // Traces sometimes capture events that don't belong to an API level queue.
                        // In that case, PAL will return ErrorIncompatibleQueue which means we should ignore
                        // the event. If we get a result that's not incompatible queue or success, then treat it
                        // as an error and break out of the loop.
                        if ((palResult != Pal::Result::ErrorIncompatibleQueue) &&
                            (palResult != Pal::Result::Success))
                        {
                            devDriverResult = DevDriver::Result::Error;
                            break;
                        }
                    }
                }

                // Free the memory for the gpu events.
                if (pGpuEvents != nullptr)
                {
                    m_pInstance->FreeMem(pGpuEvents);
                    pGpuEvents = nullptr;
                }
            }

            // Throw an assert and clean up the etw client if we fail to capture gpu events.
            // It's not a critical error though so it shouldn't abort the trace.
            if (devDriverResult != DevDriver::Result::Success)
            {
                VK_ASSERT(false);
                CleanupEtwClient();
            }
        }
#endif

        bool success = false;

        // Fetch required trace data size from GPA session
        size_t traceDataSize = 0;
        void* pTraceData     = nullptr;

        pState->pGpaSession->GetResults(pState->gpaSampleId, &traceDataSize, nullptr);

        // Allocate memory for trace data
        if (traceDataSize > 0)
        {
            pTraceData = m_pInstance->AllocMem(traceDataSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
        }

        if (pTraceData != nullptr)
        {
            // Get trace data from GPA session
            if (pState->pGpaSession->GetResults(pState->gpaSampleId, &traceDataSize, pTraceData) ==
                Pal::Result::Success)
            {
                // Transmit trace data to anyone who's listening
                auto devResult = m_pRGPServer->WriteTraceData(static_cast<Pal::uint8*>(pTraceData), traceDataSize);

                success = (devResult == DevDriver::Result::Success);
            }

            m_pInstance->FreeMem(pTraceData);
        }

        if (success)
        {
            result = Pal::Result::Success;
        }
    }

    return result;
}

// =====================================================================================================================
// Notifies of a frame-begin boundary and is used to coordinate RGP trace start/stop.
//
// If "actualPresent" is true, this is being called from an actual present; otherwise, this is manual frame-begin
// signal.
void DevModeMgr::NotifyFrameBegin(
    const Queue* pQueue,
    bool         actualPresent)
{
    // Wait for the driver to be resumed in case it's been paused.
    WaitForDriverResume();

    if ((m_pRGPServer != nullptr) && m_pRGPServer->TracesEnabled())
    {
        // Check for pending traces here also in case the application presents before submitting any work.  This
        // may transition Idle to Pending which we will handle immediately below
        //
        // Note: deliberately above the mutex lock below because PendingTraceStep() is specially written to be
        // thread-safe).
        if (m_trace.status == TraceStatus::Idle)
        {
            TraceIdleToPendingStep(&m_trace);
        }

        if (m_trace.status != TraceStatus::Idle)
        {
            Util::MutexAuto traceLock(&m_traceMutex);

            if (m_trace.status != TraceStatus::Idle)
            {
                AdvanceActiveTraceStep(&m_trace, pQueue, true, actualPresent);
            }
        }
    }
}

// =====================================================================================================================
// Returns the queue state for this particular queue.
DevModeMgr::TraceQueueState* DevModeMgr::FindTraceQueueState(TraceState* pState, const Queue* pQueue)
{
    TraceQueueState* pTraceQueue = nullptr;

    for (uint32_t queue = 0; (queue < pState->queueCount) && (pTraceQueue == nullptr); queue++)
    {
        if (pState->queueState[queue].pQueue == pQueue)
        {
            pTraceQueue = &pState->queueState[queue];
        }
    }

    return pTraceQueue;
}

// =====================================================================================================================
// Called from tracing layer before any queue submits any work.
void DevModeMgr::NotifyPreSubmit()
{
    // Check for pending traces here.
    TraceIdleToPendingStep(&m_trace);
}

// =====================================================================================================================
// This function checks for any pending traces (i.e. if the user has triggered a trace request).  It's called during
// each command buffer submit by the tracing layer and should be very light-weight.
//
// This function moves the trace state from Idle to Pending.
void DevModeMgr::TraceIdleToPendingStep(
    TraceState* pState)
{
    // Double-checked lock to test if there is a trace pending.  If so, extract its trace parameters.
    if ((m_pRGPServer != nullptr) &&
        (pState->status == TraceStatus::Idle) &&
        m_pRGPServer->IsTracePending())
    {
        Util::MutexAuto lock(&m_traceMutex);

        if (pState->status == TraceStatus::Idle)
        {
            // Update our trace parameters based on the new trace
            const auto traceParameters = m_pRGPServer->QueryTraceParameters();

            m_numPrepFrames        = traceParameters.numPreparationFrames;
            m_traceGpuMemLimit     = traceParameters.gpuMemoryLimitInMb * 1024 * 1024;
            m_enableInstTracing    = traceParameters.flags.enableInstructionTokens;
            m_allowComputePresents = traceParameters.flags.allowComputePresents;

            // Initially assume we don't need to block on trace end.  This may change during transition to
            // Preparing.
            m_blockingTraceEnd = false;

            // Store virtual frame begin/end debug object command buffer tags
            m_traceFrameBeginTag = traceParameters.beginTag;
            m_traceFrameEndTag   = traceParameters.endTag;

            // If we have valid frame begin/end tags, use the tag triggered trace mode.
            const TriggerMode triggerMode =
                ((m_traceFrameBeginTag != 0) && (m_traceFrameEndTag != 0)) ? TriggerMode::Tag
                                                                           : TriggerMode::Present;

            // Override some parameters via panel
            const RuntimeSettings& settings = pState->pDevice->GetRuntimeSettings();

            if (settings.devModeSqttPrepareFrameCount != UINT_MAX)
            {
                m_numPrepFrames = settings.devModeSqttPrepareFrameCount;
            }

            if (settings.devModeSqttTraceBeginEndTagEnable)
            {
                m_traceFrameBeginTag = settings.devModeSqttTraceBeginTagValue;
                m_traceFrameEndTag   = settings.devModeSqttTraceEndTagValue;
            }

            // Reset trace device status
            pState->preparedFrameCount = 0;
            pState->sqttFrameCount     = 0;
            pState->status             = TraceStatus::Pending;
            pState->triggerMode        = triggerMode;
        }
    }
}

// =====================================================================================================================
// This function starts preparing for an RGP trace.  Preparation involves some N frames of lead-up time during which
// timing samples are accumulated to synchronize CPU and GPU clock domains.
//
// If "actualPresent" is true, it means that this transition is happening as a consequence of an actual vkQueuePresent
// call, rather than a virtual frame boundary being communicated via API signals (e.g. debug object frame begin/end
// tags).
//
// This function transitions from the Pending state to the Preparing state.
Pal::Result DevModeMgr::TracePendingToPreparingStep(
    TraceState*  pState,
    const Queue* pQueue,
    bool         actualPresent)
{
    VK_ASSERT(pState->status  == TraceStatus::Pending);

    // If we're presenting from a compute queue and the trace parameters indicate that we want to support
    // compute queue presents, then we need to enable sample updates for this trace.  Mid-trace sample updates
    // allow us to capture a smaller set of trace data as the preparation frames run, then change the sqtt
    // token mask before the last frame to capture the full token set.  RGP requires the additional data
    // from this technique in order to handle edge cases surrounding compute queue presentation.
    m_enableSampleUpdates = m_allowComputePresents && (pQueue->PalQueue(DefaultDeviceIndex)->Type() == Pal::QueueTypeCompute);

    // We can only trace using a single device at a time currently, so recreate RGP trace
    // resources against this new one if the device is changing.
    Pal::Result result = CheckTraceDeviceChanged(pState, pQueue->VkDevice());

    Device* pDevice                 = pState->pDevice;
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    // Notify the RGP server that we are starting a trace
    if (result == Pal::Result::Success)
    {
        if (m_pRGPServer->BeginTrace() != DevDriver::Result::Success)
        {
            result = Pal::Result::ErrorUnknown;
        }
    }

    if (result == Pal::Result::Success)
    {
        result = pState->pGpaSession->Reset();
    }

    // Tell the GPA session class we're starting a trace
    if (result == Pal::Result::Success)
    {
        GpuUtil::GpaSessionBeginInfo info = {};

        info.flags.enableQueueTiming               = pState->queueTimingEnabled;
        info.flags.enableSampleUpdates             = m_enableSampleUpdates;
        info.flags.useInternalQueueSemaphoreTiming = settings.devModeSemaphoreQueueTimingEnable;

        result = pState->pGpaSession->Begin(info);
    }

    // Sample the timing clocks prior to starting a trace.
    if (result == Pal::Result::Success)
    {
        pState->pGpaSession->SampleTimingClocks();
    }

    // Find the trace queue state for this queue (the one presenting).
    TraceQueueState* pTracePrepareQueue = FindTraceQueueState(pState, pQueue);

    // If there is no compatible trace queue, fail the trace.  This should really never happen, but may possibly
    // happen if an application only requested SDMA queues in the device, or there was a catastrophic resource
    // allocation failure.
    if (pTracePrepareQueue == nullptr ||
        pTracePrepareQueue->pFamily->supportsTracing == false)
    {
        result = Pal::Result::ErrorIncompatibleQueue;
    }

    // Reset all previously used command buffers
    if (result == Pal::Result::Success)
    {
        for (uint32_t activeIdx = 0;
            (activeIdx < pState->activeCmdBufCount) && (result == Pal::Result::Success);
            activeIdx++)
        {
            pState->pActiveCmdBufs[activeIdx]->Reset(pState->pCmdAllocator, true);
        }

        pState->activeCmdBufCount  = 0;
    }

    // Build a new trace-begin command buffer
    Pal::ICmdBuffer* pBeginCmdBuf = nullptr;

    if (result == Pal::Result::Success)
    {
        pBeginCmdBuf = pTracePrepareQueue->pFamily->pTraceBeginCmdBuf;

        // Begin building the trace-begin command buffer
        Pal::CmdBufferBuildInfo info = {};

        info.flags.optimizeOneTimeSubmit = 1;

        result = pBeginCmdBuf->Begin(info);
    }

    // Start a GPA tracing sample with SQTT enabled
    if (result == Pal::Result::Success)
    {
        GpuUtil::GpaSampleConfig sampleConfig = {};

        sampleConfig.type                                = GpuUtil::GpaSampleType::Trace;
        sampleConfig.sqtt.seMask                         = UINT32_MAX;
        sampleConfig.sqtt.gpuMemoryLimit                 = m_traceGpuMemLimit;
        sampleConfig.sqtt.flags.enable                   = true;
        sampleConfig.sqtt.flags.supressInstructionTokens = (m_enableInstTracing == false);
        sampleConfig.sqtt.flags.stallMode                = Pal::GpuProfilerStallMode::GpuProfilerStallAlways;

        // Override trace buffer size from panel
        if (settings.devModeSqttGpuMemoryLimit != 0)
        {
            sampleConfig.sqtt.gpuMemoryLimit = settings.devModeSqttGpuMemoryLimit;
        }

        pState->gpaSampleId = pState->pGpaSession->BeginSample(pBeginCmdBuf, sampleConfig);
    }

    // Finish building the trace-begin command buffer
    if (result == Pal::Result::Success)
    {
        result = pBeginCmdBuf->End();
    }

#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
    if (result == Pal::Result::Success)
    {
        // Enable tracing on the ETW client if it's connected.
        if (m_pEtwClient != nullptr)
        {
            const DevDriver::Result devDriverResult = m_pEtwClient->EnableTracing(Util::GetIdOfCurrentProcess());

            // If an error occurs, cleanup the ETW client so it doesn't continue to cause issues for future traces.
            if (devDriverResult != DevDriver::Result::Success)
            {
                VK_ASSERT(false);
                CleanupEtwClient();
            }
        }
    }
#endif

    // Reset the trace-begin fence
    if (result == Pal::Result::Success)
    {
        // Register this as an active command buffer
        VK_ASSERT(pState->activeCmdBufCount < VK_ARRAY_SIZE(pState->pActiveCmdBufs));

        pState->pActiveCmdBufs[pState->activeCmdBufCount++] = pBeginCmdBuf;

        result = pDevice->PalDevice(DefaultDeviceIndex)->ResetFences(1, &pState->pBeginFence);
    }

    // If we're enabling sample updates, we need to prepare the begin sqtt command buffer now and also submit the
    // regular begin command buffer.
    if (m_enableSampleUpdates)
    {
        // Build a new trace-begin-sqtt command buffer
        Pal::ICmdBuffer* pBeginSqttCmdBuf = nullptr;

        if (result == Pal::Result::Success)
        {
            pBeginSqttCmdBuf = pTracePrepareQueue->pFamily->pTraceBeginSqttCmdBuf;

            // Begin building the trace-begin-sqtt command buffer
            Pal::CmdBufferBuildInfo info = {};

            info.flags.optimizeOneTimeSubmit = 1;

            result = pBeginSqttCmdBuf->Begin(info);
        }

        // Use GpaSession to update the sqtt token mask via UpdateSampleTraceParams.
        if (result == Pal::Result::Success)
        {
            pState->pGpaSession->UpdateSampleTraceParams(pBeginSqttCmdBuf, pState->gpaSampleId, GpuUtil::UpdateSampleTraceMode::MinimalToFullMask);
        }

        // Finish building the trace-begin-sqtt command buffer
        if (result == Pal::Result::Success)
        {
            result = pBeginSqttCmdBuf->End();
        }

        if (result == Pal::Result::Success)
        {
            // Register this as an active command buffer
            VK_ASSERT(pState->activeCmdBufCount < VK_ARRAY_SIZE(pState->pActiveCmdBufs));

            pState->pActiveCmdBufs[pState->activeCmdBufCount++] = pBeginSqttCmdBuf;
        }

        // Submit the trace-begin command buffer
        if (result == Pal::Result::Success)
        {
            Pal::SubmitInfo submitInfo = {};

            submitInfo.cmdBufferCount = 1;
            submitInfo.ppCmdBuffers = &pTracePrepareQueue->pFamily->pTraceBeginCmdBuf;
            submitInfo.pFence = nullptr;

            result = pQueue->PalQueue(DefaultDeviceIndex)->Submit(submitInfo);
        }
    }

    if (result == Pal::Result::Success)
    {
        // Remember which queue started the trace
        pState->pTracePrepareQueue = pTracePrepareQueue;
        pState->pTraceBeginQueue   = nullptr;
        pState->pTraceEndQueue     = nullptr;
        pState->pTraceEndSqttQueue = nullptr;

        m_trace.status = TraceStatus::Preparing;

        // If the app is not tracing using vkQueuePresent, we need to immediately block on trace end
        // (rather than periodically checking during future present calls).
        m_blockingTraceEnd |= (actualPresent == false);

        // Override via panel setting
        m_blockingTraceEnd |= settings.devModeSqttForceBlockOnTraceEnd;
    }
    else
    {
        // We failed to prepare for the trace so abort it.
        if (m_pRGPServer != nullptr)
        {
            const DevDriver::Result devDriverResult = m_pRGPServer->AbortTrace();

            // AbortTrace should always succeed unless we've used the api incorrectly.
            VK_ASSERT(devDriverResult == DevDriver::Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// This function begins an RGP trace by initializing all dependent resources and submitting the "begin trace"
// information command buffer which starts SQ thread tracing (SQTT).
//
// This function transitions from the Preparing state to the Running state.
Pal::Result DevModeMgr::TracePreparingToRunningStep(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::Preparing);
    VK_ASSERT(m_tracingEnabled);

    // We can only trace using a single device at a time currently, so recreate RGP trace
    // resources against this new one if the device is changing.
    Pal::Result result = CheckTraceDeviceChanged(pState, pQueue->VkDevice());

    if (result == Pal::Result::Success)
    {
        // Take a calibration timing measurement sample for this frame.
        m_trace.pGpaSession->SampleTimingClocks();

        // Start the SQTT trace if we've waited a sufficient number of preparation frames
        if (m_trace.preparedFrameCount >= m_numPrepFrames)
        {
            TraceQueueState* pTraceQueue = nullptr;

            if (result == Pal::Result::Success)
            {
                pTraceQueue = FindTraceQueueState(pState, pQueue);

                // Only allow trace to start if the queue family at prep-time matches the queue
                // family at begin time because the command buffer engine type must match
                if ((pTraceQueue == nullptr) ||
                    (pTraceQueue->pFamily->supportsTracing == false) ||
                    (pState->pTracePrepareQueue == nullptr) ||
                    (pTraceQueue->pFamily != pState->pTracePrepareQueue->pFamily))
                {
                    result = Pal::Result::ErrorIncompatibleQueue;
                }
            }

            // Optionally execute a device wait idle if panel says so
            if ((result == Pal::Result::Success) &&
                pState->pDevice->GetRuntimeSettings().devModeSqttWaitIdle)
            {
                pState->pDevice->WaitIdle();
            }

            // Submit the trace-begin command buffer
            if (result == Pal::Result::Success)
            {
                Pal::SubmitInfo submitInfo = {};

                submitInfo.cmdBufferCount = 1;
                submitInfo.ppCmdBuffers   = m_enableSampleUpdates ? &pTraceQueue->pFamily->pTraceBeginSqttCmdBuf
                                                                  : &pTraceQueue->pFamily->pTraceBeginCmdBuf;
                submitInfo.pFence         = pState->pBeginFence;

                result = pQueue->PalQueue(DefaultDeviceIndex)->Submit(submitInfo);
            }

            // Make the trace active and remember which queue started it
            if (result == Pal::Result::Success)
            {
                pState->status           = TraceStatus::Running;
                pState->pTraceBeginQueue = pTraceQueue;
            }
        }
        // Flush all queues on the last preparation frame.
        //
        // We only need this if mid-trace sample updates are enabled and the driver setting for flushing queues
        // is also enabled. This is used to provide RGP with a guaranteed idle point in the thread trace data.
        // That point can be used to synchronize the hardware pipeline stages in the sqtt parsing logic.
        else if ((m_trace.preparedFrameCount == (m_numPrepFrames - 1)) &&
                  m_enableSampleUpdates                                &&
                  m_trace.flushAllQueues)
        {
            for (uint32_t family = 0; family < m_trace.queueFamilyCount; ++family)
            {
                TraceQueueFamilyState* pFamilyState = &m_trace.queueFamilyState[family];

                if (pFamilyState->supportsTracing)
                {
                    // If the queue family supports tracing, then find a queue that we can flush on.
                    for (uint32_t queueIndex = 0; queueIndex < m_trace.queueCount; ++queueIndex)
                    {
                        TraceQueueState* pQueueState = &m_trace.queueState[queueIndex];

                        if (pQueueState->pFamily == pFamilyState)
                        {
                            // Submit the flush command buffer
                            Pal::SubmitInfo submitInfo = {};

                            submitInfo.cmdBufferCount = 1;
                            submitInfo.ppCmdBuffers   = &pFamilyState->pTraceFlushCmdBuf;
                            submitInfo.pFence         = nullptr;

                            result = pQueueState->pQueue->PalQueue(DefaultDeviceIndex)->Submit(submitInfo);

                            break;
                        }
                    }
                }

                // Break out of the loop if we encounter an error.
                if (result != Pal::Result::Success)
                {
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function submits the command buffer to stop SQTT tracing.  Full tracing still continues.
//
// This function transitions from the Running state to the WaitingForSqtt state.
Pal::Result DevModeMgr::TraceRunningToWaitingForSqttStep(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::Running);

    // Do not advance unless we've traced the necessary number of frames
    if (m_trace.sqttFrameCount < m_trace.pDevice->GetRuntimeSettings().devModeSqttFrameCount)
    {
        return Pal::Result::Success;
    }

    Pal::Result result = Pal::Result::Success;

    // Find the trace queue state for this queue (the one presenting).
    TraceQueueState* pTraceQueue = FindTraceQueueState(pState, pQueue);

    // Only allow SQTT trace to start and end on the same queue because it's critical that these are
    // in the same order
    if ((pTraceQueue == nullptr) || (pTraceQueue != pState->pTraceBeginQueue))
    {
        result = Pal::Result::ErrorIncompatibleQueue;
    }

    Pal::IDevice* pPalDevice        = nullptr;
    Pal::ICmdBuffer* pEndSqttCmdBuf = nullptr;

    if (result == Pal::Result::Success)
    {
        // Start building the trace-end command buffer
        pPalDevice     = pState->pDevice->PalDevice(DefaultDeviceIndex);
        pEndSqttCmdBuf = pTraceQueue->pFamily->pTraceEndSqttCmdBuf;

        Pal::CmdBufferBuildInfo buildInfo = {};

        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pEndSqttCmdBuf->Begin(buildInfo);
    }

    // Tell the GPA session to insert any necessary commands to end the tracing sample and end the session itself
    if (result == Pal::Result::Success)
    {
        VK_ASSERT(pState->pGpaSession != nullptr);

        pState->pGpaSession->EndSample(pEndSqttCmdBuf, pState->gpaSampleId);
    }

    // Finish building the trace-end command buffer
    if (result == Pal::Result::Success)
    {
        result = pEndSqttCmdBuf->End();
    }

    // Reset the trace-end-sqtt fence
    if (result == Pal::Result::Success)
    {
        // Register this as an active command buffer
        VK_ASSERT(pState->activeCmdBufCount < VK_ARRAY_SIZE(pState->pActiveCmdBufs));

        pState->pActiveCmdBufs[pState->activeCmdBufCount++] = pEndSqttCmdBuf;

        result = pPalDevice->ResetFences(1, &pState->pEndSqttFence);
    }

    // Submit the trace-end-sqtt command buffer
    if (result == Pal::Result::Success)
    {
        Pal::SubmitInfo submitInfo = {};

        submitInfo.cmdBufferCount = 1;
        submitInfo.ppCmdBuffers   = &pEndSqttCmdBuf;
        submitInfo.pFence         = pState->pEndSqttFence;

        result = pQueue->PalQueue(DefaultDeviceIndex)->Submit(submitInfo);
    }

    // Optionally execute a device wait idle if panel says so
    if ((result == Pal::Result::Success) &&
        pState->pDevice->GetRuntimeSettings().devModeSqttWaitIdle)
    {
        pState->pDevice->WaitIdle();
    }

    if (result == Pal::Result::Success)
    {
        pState->status = TraceStatus::WaitingForSqtt;
    }

    return result;
}

// =====================================================================================================================
// This function ends a running RGP trace.
//
// This function transitions from the WaitingForSqtt state to WaitingForResults state.
Pal::Result DevModeMgr::TraceWaitingForSqttToEndingStep(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::WaitingForSqtt);

    // Check if the SQTT-end fence has signaled yet.
    Pal::Result fenceResult = pState->pEndSqttFence->GetStatus();

    if (fenceResult == Pal::Result::NotReady && m_blockingTraceEnd)
    {
        fenceResult = pState->pDevice->PalDevice(DefaultDeviceIndex)->WaitForFences(1, &pState->pEndSqttFence, true, InfiniteTimeout);
    }

    // Return without advancing if not ready yet or submit failed
    if (fenceResult == Pal::Result::NotReady)
    {
        return Pal::Result::Success;
    }
    else if (fenceResult != Pal::Result::Success)
    {
        return fenceResult;
    }

    Pal::Result result = Pal::Result::Success;

    // Find the trace queue state for this queue (the one presenting).
    TraceQueueState* pTraceQueue = FindTraceQueueState(pState, pQueue);

    if (pTraceQueue == nullptr)
    {
        result = Pal::Result::ErrorIncompatibleQueue;
    }

    Pal::IDevice* pPalDevice    = nullptr;
    Pal::ICmdBuffer* pEndCmdBuf = nullptr;

    if (result == Pal::Result::Success)
    {
        pPalDevice = pState->pDevice->PalDevice(DefaultDeviceIndex);
        pEndCmdBuf = pTraceQueue->pFamily->pTraceEndCmdBuf;
    }

    // Start building the trace-end command buffer
    if (result == Pal::Result::Success)
    {
        Pal::CmdBufferBuildInfo buildInfo = {};

        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pEndCmdBuf->Begin(buildInfo);
    }

    // Tell the GPA session to insert any necessary commands to end the tracing sample and end the session itself
    if (result == Pal::Result::Success)
    {
        VK_ASSERT(pState->pGpaSession != nullptr);

        result = pState->pGpaSession->End(pEndCmdBuf);
    }

    // Finish building the trace-end command buffer
    if (result == Pal::Result::Success)
    {
        result = pEndCmdBuf->End();
    }

    // Reset the trace-end fence
    if (result == Pal::Result::Success)
    {
        // Register this as an active command buffer
        VK_ASSERT(pState->activeCmdBufCount < VK_ARRAY_SIZE(pState->pActiveCmdBufs));

        pState->pActiveCmdBufs[pState->activeCmdBufCount++] = pEndCmdBuf;

        result = pPalDevice->ResetFences(1, &pState->pEndFence);
    }

    // Submit the trace-end command buffer
    if (result == Pal::Result::Success)
    {
        Pal::SubmitInfo submitInfo = {};

        submitInfo.cmdBufferCount = 1;
        submitInfo.ppCmdBuffers   = &pEndCmdBuf;
        submitInfo.pFence         = pState->pEndFence;

        result = pQueue->PalQueue(DefaultDeviceIndex)->Submit(submitInfo);
    }

    if (result == Pal::Result::Success)
    {
        pState->status         = TraceStatus::Ending;
        pState->pTraceEndQueue = pTraceQueue;
    }

    return result;
}

// =====================================================================================================================
// This function resets and possibly cancels a currently active (between begin/end) RGP trace.  It frees any dependent
// resources.
void DevModeMgr::FinishOrAbortTrace(
    TraceState* pState,
    bool        aborted)
{
    DevDriver::RGPProtocol::RGPServer* m_pRGPServer = m_pDevDriverServer->GetRGPServer();

    VK_ASSERT(m_pRGPServer != nullptr);

    // Inform RGP protocol that we're done with the trace, either by aborting it or finishing normally
    if (aborted)
    {
        m_pRGPServer->AbortTrace();
    }
    else
    {
        m_pRGPServer->EndTrace();
    }

    if (pState->pGpaSession != nullptr)
    {
        pState->pGpaSession->Reset();
    }

#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
    // Disable tracing on the ETW client if it exists.
    if (aborted && m_pEtwClient != nullptr)
    {
        DevDriver::Result devDriverResult = m_pEtwClient->DisableTracing(nullptr);
        VK_ASSERT(devDriverResult == DevDriver::Result::Success);
    }
#endif

    // Reset tracing state to idle
    pState->preparedFrameCount = 0;
    pState->sqttFrameCount     = 0;
    pState->gpaSampleId        = 0;
    pState->status             = TraceStatus::Idle;
    pState->triggerMode        = TriggerMode::Present;
    pState->pTracePrepareQueue = nullptr;
    pState->pTraceBeginQueue   = nullptr;
    pState->pTraceEndQueue     = nullptr;
    pState->pTraceEndSqttQueue = nullptr;
}

// =====================================================================================================================
// This function will reinitialize RGP tracing resources that are reused between traces if the new trace device
// has changed since the last trace.
Pal::Result DevModeMgr::CheckTraceDeviceChanged(
    TraceState* pState,
    Device*     pNewDevice)
{
    Pal::Result result = Pal::Result::Success;

    if (pState->pDevice != pNewDevice)
    {
        // If we are idle or pending, we can re-initialize trace resources based on the new device.
        if (pState->status == TraceStatus::Idle || pState->status == TraceStatus::Pending)
        {
            DestroyRGPTracing(pState);

            if (pNewDevice != nullptr)
            {
                result = InitRGPTracing(pState, pNewDevice);
            }
        }
        // Otherwise, we're switching devices in the middle of a trace and have to fail.
        else
        {
            result = Pal::Result::ErrorIncompatibleDevice;
        }
    }

    return result;
}

// =====================================================================================================================
// Destroys device-persistent RGP resources for a particular queue family
void DevModeMgr::DestroyTraceQueueFamilyResources(TraceQueueFamilyState* pState)
{
    if (pState->pTraceBeginCmdBuf != nullptr)
    {
        pState->pTraceBeginCmdBuf->Destroy();
        m_pInstance->FreeMem(pState->pTraceBeginCmdBuf);
        pState->pTraceBeginCmdBuf = nullptr;
    }

    if (pState->pTraceBeginSqttCmdBuf != nullptr)
    {
        pState->pTraceBeginSqttCmdBuf->Destroy();
        m_pInstance->FreeMem(pState->pTraceBeginSqttCmdBuf);
        pState->pTraceBeginSqttCmdBuf = nullptr;
    }

    if (pState->pTraceFlushCmdBuf != nullptr)
    {
        pState->pTraceFlushCmdBuf->Destroy();
        m_pInstance->FreeMem(pState->pTraceFlushCmdBuf);
        pState->pTraceFlushCmdBuf = nullptr;
    }

    if (pState->pTraceEndSqttCmdBuf != nullptr)
    {
        pState->pTraceEndSqttCmdBuf->Destroy();
        m_pInstance->FreeMem(pState->pTraceEndSqttCmdBuf);
        pState->pTraceEndSqttCmdBuf = nullptr;
    }

    if (pState->pTraceEndCmdBuf != nullptr)
    {
        pState->pTraceEndCmdBuf->Destroy();
        m_pInstance->FreeMem(pState->pTraceEndCmdBuf);
        pState->pTraceEndCmdBuf = nullptr;
    }
}

// =====================================================================================================================
// Destroys device-persistent RGP resources
void DevModeMgr::DestroyRGPTracing(TraceState* pState)
{
    if (pState->status != TraceStatus::Idle)
    {
        FinishOrAbortTrace(pState, true);
    }

    // Destroy the GPA session
    if (pState->pGpaSession != nullptr)
    {
        Util::Destructor(pState->pGpaSession);
        m_pInstance->FreeMem(pState->pGpaSession);
        pState->pGpaSession = nullptr;
    }

    if (pState->pBeginFence != nullptr)
    {
        pState->pBeginFence->Destroy();
        m_pInstance->FreeMem(pState->pBeginFence);
    }

    if (pState->pEndSqttFence != nullptr)
    {
        pState->pEndSqttFence->Destroy();
        m_pInstance->FreeMem(pState->pEndSqttFence);
    }

    if (pState->pEndFence != nullptr)
    {
        pState->pEndFence->Destroy();
        m_pInstance->FreeMem(pState->pEndFence);
    }

    for (uint32_t family = 0; family < pState->queueFamilyCount; ++family)
    {
        DestroyTraceQueueFamilyResources(&pState->queueFamilyState[family]);
    }

    if (pState->pCmdAllocator != nullptr)
    {
        pState->pCmdAllocator->Destroy();
        m_pInstance->FreeMem(pState->pCmdAllocator);
    }

    pState->queueCount       = 0;
    pState->queueFamilyCount = 0;

    memset(pState, 0, sizeof(*pState));
}

// =====================================================================================================================
// This function finds out all the queues in the device that we have to synchronize for RGP-traced frames and
// initializes resources for them.
Pal::Result DevModeMgr::InitTraceQueueResources(
    TraceState* pState,
    bool*       pHasDebugVmid)
{
    VK_ASSERT(pState->queueCount == 0);
    VK_ASSERT(pState->queueFamilyCount == 0);
    VK_ASSERT(pState->pGpaSession != nullptr);

    Pal::Result result = Pal::Result::Success;

    if (pState->pDevice != nullptr)
    {
        for (uint32_t familyIdx = 0; familyIdx < Queue::MaxQueueFamilies; ++familyIdx)
        {
            for (uint32_t queueIdx = 0;
                (queueIdx < Queue::MaxQueuesPerFamily) && (result == Pal::Result::Success);
                ++queueIdx)
            {
                VkQueue queueHandle;

                if ((pState->pDevice->GetQueue(familyIdx, queueIdx, &queueHandle) == VK_SUCCESS) &&
                    (queueHandle != VK_NULL_HANDLE))
                {
                    Queue* pQueue = ApiQueue::ObjectFromHandle(queueHandle);

                    // Has this queue's family been previously seen?
                    TraceQueueFamilyState* pFamilyState = nullptr;

                    for (uint32_t familyStateIdx = 0; familyStateIdx < pState->queueFamilyCount; ++familyStateIdx)
                    {
                        if (pState->queueFamilyState[familyStateIdx].queueFamilyIndex == familyIdx)
                        {
                            pFamilyState = &pState->queueFamilyState[familyStateIdx];
                        }
                    }

                    // Figure out information about this queue's family if it hasn't been seen before
                    if (pFamilyState == nullptr)
                    {
                        VK_ASSERT(pState->queueFamilyCount < VK_ARRAY_SIZE(pState->queueFamilyState));

                        pFamilyState = &pState->queueFamilyState[pState->queueFamilyCount++];

                        pFamilyState->queueFamilyIndex = familyIdx;
                        pFamilyState->supportsTracing  = SqttMgr::IsTracingSupported(
                                                            pState->pDevice->VkPhysicalDevice(DefaultDeviceIndex), familyIdx);
                        pFamilyState->queueType        = pState->pDevice->GetQueueFamilyPalQueueType(familyIdx);
                        pFamilyState->engineType       = pState->pDevice->GetQueueFamilyPalEngineType(familyIdx);

                        // Initialize resources for this queue family
                        result = InitTraceQueueFamilyResources(pState, pFamilyState);
                    }

                    // Register this queue for timing operations
                    if (result == Pal::Result::Success)
                    {
                        VK_ASSERT(pState->queueCount < VK_ARRAY_SIZE(pState->queueState));

                        TraceQueueState* pQueueState = &pState->queueState[pState->queueCount++];

                        pQueueState->pQueue          = pQueue;
                        pQueueState->pFamily         = pFamilyState;
                        pQueueState->timingSupported = false;
                        pQueueState->queueId         = reinterpret_cast<Pal::uint64>(queueHandle);

                        // Get the OS context handle for this queue (this is a thing that RGP needs on DX clients;
                        // it may be optional for Vulkan, but we provide it anyway if available).
                        Pal::KernelContextInfo kernelContextInfo = {};

                        Pal::Result palResult = Pal::Result::Success;
                        Pal::Result queryKernelSuccess = pQueue->PalQueue(DefaultDeviceIndex)->QueryKernelContextInfo(&kernelContextInfo);

                        // Ensure we've acquired the debug VMID (note that some platforms do not
                        // implement this function, so don't fail the whole trace if so)
                        if (queryKernelSuccess == Pal::Result::Success &&
                            kernelContextInfo.flags.hasDebugVmid == false)
                        {
                            *pHasDebugVmid = false;
                        }

                        if (pState->queueTimingEnabled)
                        {
                            if (palResult == Pal::Result::Success)
                            {
                                pQueueState->queueContext = kernelContextInfo.contextIdentifier;
                            }

                            // I think we need a GPA session per PAL device in the group, and we need to register each
                            // per-device queue with the corresponding PAL device's GPA session.  This needs to be
                            // fixed for MGPU tracing to work (among probably many other things).
                            VK_ASSERT(pState->pDevice->NumPalDevices() == 1);

                            // Register the queue with the GPA session class for timed queue operation support.
                            if (pState->pGpaSession->RegisterTimedQueue(
                                pQueue->PalQueue(DefaultDeviceIndex),
                                pQueueState->queueId,
                                pQueueState->queueContext) == Pal::Result::Success)
                            {
                                pQueueState->timingSupported = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function initializes the queue family -specific resources to support RGP tracing for a particular queue family
Pal::Result DevModeMgr::InitTraceQueueFamilyResources(
    TraceState*            pTraceState,
    TraceQueueFamilyState* pFamilyState)
{
    Pal::Result result = Pal::Result::Success;

    // Test if this queue type supports SQ thread tracing
    if (pFamilyState->supportsTracing)
    {
        Pal::IDevice* pPalDevice = pTraceState->pDevice->PalDevice(DefaultDeviceIndex);

        Pal::CmdBufferCreateInfo createInfo = {};

        createInfo.pCmdAllocator = pTraceState->pCmdAllocator;
        createInfo.queueType     = pFamilyState->queueType;
        createInfo.engineType    = pFamilyState->engineType;

        const size_t cmdBufferSize = pPalDevice->GetCmdBufferSize(createInfo, nullptr);

        // Create trace-begin command buffer
        void* pStorage = m_pInstance->AllocMem(cmdBufferSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            result = pPalDevice->CreateCmdBuffer(createInfo, pStorage, &pFamilyState->pTraceBeginCmdBuf);

            if (result != Pal::Result::Success)
            {
                m_pInstance->FreeMem(pStorage);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }

        // Create trace-begin-sqtt SQTT command buffer
        if (result == Pal::Result::Success)
        {
            pStorage = m_pInstance->AllocMem(cmdBufferSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

            if (pStorage != nullptr)
            {
                result = pPalDevice->CreateCmdBuffer(createInfo, pStorage, &pFamilyState->pTraceBeginSqttCmdBuf);

                if (result != Pal::Result::Success)
                {
                    m_pInstance->FreeMem(pStorage);
                }
            }
            else
            {
                result = Pal::Result::ErrorOutOfMemory;
            }
        }

        // Create trace-end SQTT command buffer
        if (result == Pal::Result::Success)
        {
            pStorage = m_pInstance->AllocMem(cmdBufferSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

            if (pStorage != nullptr)
            {
                result = pPalDevice->CreateCmdBuffer(createInfo, pStorage, &pFamilyState->pTraceEndSqttCmdBuf);

                if (result != Pal::Result::Success)
                {
                    m_pInstance->FreeMem(pStorage);
                }
            }
            else
            {
                result = Pal::Result::ErrorOutOfMemory;
            }
        }

        // Create trace-end command buffer
        if (result == Pal::Result::Success)
        {
            pStorage = m_pInstance->AllocMem(cmdBufferSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

            if (pStorage != nullptr)
            {
                result = pPalDevice->CreateCmdBuffer(createInfo, pStorage, &pFamilyState->pTraceEndCmdBuf);

                if (result != Pal::Result::Success)
                {
                    m_pInstance->FreeMem(pStorage);
                }
            }
            else
            {
                result = Pal::Result::ErrorOutOfMemory;
            }
        }

        // Prepare the flush command buffer resources if necessary.
        if (pTraceState->flushAllQueues)
        {
            // Create trace-flush command buffer
            if (result == Pal::Result::Success)
            {
                pStorage = m_pInstance->AllocMem(cmdBufferSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

                if (pStorage != nullptr)
                {
                    result = pPalDevice->CreateCmdBuffer(createInfo, pStorage, &pFamilyState->pTraceFlushCmdBuf);

                    if (result != Pal::Result::Success)
                    {
                        m_pInstance->FreeMem(pStorage);
                    }
                }
                else
                {
                    result = Pal::Result::ErrorOutOfMemory;
                }
            }

            // Record the new trace-flush command buffer
            Pal::ICmdBuffer* pTraceFlushCmdBuf = pFamilyState->pTraceFlushCmdBuf;

            if (result == Pal::Result::Success)
            {
                // Begin building the trace-flush command buffer
                Pal::CmdBufferBuildInfo info = {};

                info.flags.optimizeOneTimeSubmit = 1;

                result = pTraceFlushCmdBuf->Begin(info);
            }

            // Record a full pipeline flush into the command barrier
            if (result == Pal::Result::Success)
            {
                const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;
                Pal::BarrierInfo barrierInfo = {};

                // This code by definition does not execute during SQ thread tracing so this barrier doesn't need to be
                // identified.
                barrierInfo.reason              = RgpBarrierUnknownReason;
                barrierInfo.waitPoint           = Pal::HwPipeTop;
                barrierInfo.pipePointWaitCount  = 1;
                barrierInfo.pPipePoints         = &pipePoint;

                pTraceFlushCmdBuf->CmdBarrier(barrierInfo);
            }

            // Finish building the trace-flush command buffer
            if (result == Pal::Result::Success)
            {
                result = pTraceFlushCmdBuf->End();
            }
        }
    }

    // If something went wrong in resource creation, clean up and disable tracing for this queue family
    if (result != Pal::Result::Success)
    {
        DestroyTraceQueueFamilyResources(pFamilyState);

        pFamilyState->supportsTracing = false;
    }

    return result;
}

// =====================================================================================================================
// Returns true if the given device properties/settings support tracing.
bool DevModeMgr::GpuSupportsTracing(
    const Pal::DeviceProperties& props,
    const RuntimeSettings&       settings)
{
    return props.gfxipProperties.flags.supportRgpTraces &&
           (settings.devModeSqttForceDisable == false);
}

// =====================================================================================================================
// Initializes device-persistent RGP resources
Pal::Result DevModeMgr::InitRGPTracing(
    TraceState* pState,
    Device*     pDevice)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    Pal::Result result = Pal::Result::Success;

    if ((m_tracingEnabled == false) ||  // Tracing is globally disabled
        (m_pRGPServer == nullptr) ||    // There is no RGP server (this should never happen)
        (pDevice->NumPalDevices() > 1)) // MGPU device group tracing is not currently supported
    {
        result = Pal::Result::ErrorInitializationFailed;
    }

    // Fail initialization of trace resources if SQTT tracing has been force-disabled from the panel (this will
    // consequently fail the trace), or if the chosen device's gfxip does not support SQTT.
    //
    // It's necessary to check this during RGP tracing init in addition to devmode init because during the earlier
    // devmode init we may be in a situation where some enumerated physical devices support tracing and others do not.
    if (GpuSupportsTracing(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties(), pDevice->GetRuntimeSettings()) == false)
    {
        result = Pal::Result::ErrorInitializationFailed;
    }

    if (result == Pal::Result::Success)
    {
        VK_ASSERT(pState->pDevice == nullptr);

        pState->queueTimingEnabled = settings.devModeQueueTimingEnable;
        pState->flushAllQueues     = settings.devModeSqttFlushAllQueues;
        pState->pDevice            = pDevice;
    }

    Pal::IDevice* pPalDevice = pDevice->PalDevice(DefaultDeviceIndex);

    // Create a command buffer allocator for the RGP tracing command buffers
    if (result == Pal::Result::Success)
    {
        Pal::CmdAllocatorCreateInfo createInfo = {};

        createInfo.flags.threadSafe               = 1;
        createInfo.flags.autoMemoryReuse          = 1;
        createInfo.flags.disableBusyChunkTracking = 1;

        // Initialize command data chunk allocation size
        createInfo.allocInfo[Pal::CommandDataAlloc].allocHeap    = settings.cmdAllocatorDataHeap;
        createInfo.allocInfo[Pal::CommandDataAlloc].allocSize    = settings.cmdAllocatorDataAllocSize;
        createInfo.allocInfo[Pal::CommandDataAlloc].suballocSize = settings.cmdAllocatorDataSubAllocSize;

        // Initialize embedded data chunk allocation size
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocHeap    = settings.cmdAllocatorEmbeddedHeap;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].allocSize    = settings.cmdAllocatorEmbeddedAllocSize;
        createInfo.allocInfo[Pal::EmbeddedDataAlloc].suballocSize = settings.cmdAllocatorEmbeddedSubAllocSize;

        // Initialize GPU scratch memory chunk allocation size
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocHeap    = settings.cmdAllocatorScratchHeap;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].allocSize    = settings.cmdAllocatorScratchAllocSize;
        createInfo.allocInfo[Pal::GpuScratchMemAlloc].suballocSize = settings.cmdAllocatorScratchSubAllocSize;

        const size_t allocatorSize = pPalDevice->GetCmdAllocatorSize(createInfo, nullptr);

        void* pStorage = m_pInstance->AllocMem(allocatorSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            result = pPalDevice->CreateCmdAllocator(createInfo, pStorage, &pState->pCmdAllocator);

            if (result != Pal::Result::Success)
            {
                m_pInstance->FreeMem(pStorage);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    if (result == Pal::Result::Success)
    {
        const size_t fenceSize = pPalDevice->GetFenceSize(nullptr);

        // Create trace-begin command buffer fence
        void* pStorage = m_pInstance->AllocMem(fenceSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            Pal::FenceCreateInfo createInfo = {};
            result = pPalDevice->CreateFence(createInfo, pStorage, &pState->pBeginFence);

            if (result != Pal::Result::Success)
            {
                m_pInstance->FreeMem(pStorage);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }

        // Create trace-end-sqtt command buffer fence
        pStorage = m_pInstance->AllocMem(fenceSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            Pal::FenceCreateInfo createInfo = {};
            result = pPalDevice->CreateFence(createInfo, pStorage, &pState->pEndSqttFence);

            if (result != Pal::Result::Success)
            {
                m_pInstance->FreeMem(pStorage);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }

        // Create trace-end command buffer fence
        pStorage = m_pInstance->AllocMem(fenceSize, VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            Pal::FenceCreateInfo createInfo = {};

            result = pPalDevice->CreateFence(createInfo, pStorage, &pState->pEndFence);

            if (result != Pal::Result::Success)
            {
                m_pInstance->FreeMem(pStorage);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    // Create a GPA session object for this trace session
    if (result == Pal::Result::Success)
    {
        VK_ASSERT(pState->pGpaSession == nullptr);

        void* pStorage = m_pInstance->AllocMem(sizeof(GpuUtil::GpaSession), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            const uint32_t apiVersion = m_pInstance->GetAPIVersion();

            pState->pGpaSession = VK_PLACEMENT_NEW(pStorage) GpuUtil::GpaSession(
                m_pInstance->PalPlatform(),
                pPalDevice,
                VK_VERSION_MAJOR(apiVersion),
                VK_VERSION_MINOR(apiVersion),
                RgpSqttInstrumentationSpecVersion,
                RgpSqttInstrumentationApiVersion);
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    // Initialize the GPA session
    if (result == Pal::Result::Success)
    {
        result = pState->pGpaSession->Init();
    }

    // Initialize trace resources required by each queue (and queue family)
    bool hasDebugVmid = true;

    if (result == Pal::Result::Success)
    {
        result = InitTraceQueueResources(pState, &hasDebugVmid);
    }

    // If we've failed to acquire the debug VMID, fail to trace
    if (hasDebugVmid == false)
    {
        result = Pal::Result::ErrorInitializationFailed;
    }

#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
    // Attempt to initialize the ETW client for use with RGP traces. This might fail if there's no ETW server
    // available on the message bus. Failure just means that we won't be able to get extra information about
    // queue signal and wait events. Just trigger an assert in the case of failure.
    if ((result == Pal::Result::Success) &&
        (settings.devModeSemaphoreQueueTimingEnable == false))
    {
        Pal::Result etwInitResult = InitEtwClient();
        VK_ASSERT(etwInitResult == Pal::Result::Success);
    }
#endif

    if (result != Pal::Result::Success)
    {
        // If we've failed to initialize tracing, permanently disable traces
        if (m_pRGPServer != nullptr)
        {
            m_pRGPServer->DisableTraces();

            m_tracingEnabled = false;
        }

        // Clean up if we failed
        DestroyRGPTracing(pState);
    }

    return result;
}

// =====================================================================================================================
// Called when a new device is created.  This will preallocate reusable RGP trace resources for that device.
void DevModeMgr::PostDeviceCreate(Device* pDevice)
{
    Util::MutexAuto lock(&m_traceMutex);

    // Pre-allocate trace resources for this device
    CheckTraceDeviceChanged(&m_trace, pDevice);

    auto* pDriverControlServer = m_pDevDriverServer->GetDriverControlServer();

    VK_ASSERT(pDriverControlServer != nullptr);

    // If the driver hasn't been marked as fully initialized yet, mark it now. We consider the time after the logical
    // device creation to be the fully initialized driver position. This is mainly because PAL is fully initialized
    // at this point and we also know whether or not the debug vmid has been acquired. External tools use this
    // information to decide when it's reasonable to make certain requests of the driver through protocol functions.
    if (pDriverControlServer->IsDriverInitialized() == false)
    {
        pDriverControlServer->FinishDriverInitialization();
    }
}

// =====================================================================================================================
// Called prior to a device's being destroyed.  This will free persistent RGP trace resources for that device.
void DevModeMgr::PreDeviceDestroy(Device* pDevice)
{
    Util::MutexAuto lock(&m_traceMutex);

    if (m_trace.pDevice == pDevice)
    {
        // Free trace resources
        CheckTraceDeviceChanged(&m_trace, nullptr);
    }
}

// =====================================================================================================================
bool DevModeMgr::QueueSupportsTiming(
    uint32_t     deviceIdx,
    const Queue* pQueue)
{
    VK_ASSERT(IsQueueTimingActive(pQueue->VkDevice()));
    VK_ASSERT(deviceIdx == DefaultDeviceIndex); // MGPU tracing is not supported

    bool timingSupported = (deviceIdx == DefaultDeviceIndex) &&
                           (pQueue->VkDevice() == m_trace.pDevice);

    // Make sure this queue was successfully registered
    if (timingSupported)
    {
        const TraceQueueState* pTraceQueueState = FindTraceQueueState(&m_trace, pQueue);

        if (pTraceQueueState == nullptr || pTraceQueueState->timingSupported == false)
        {
            timingSupported = false;
        }
    }

    return timingSupported;
}

// =====================================================================================================================
Pal::Result DevModeMgr::TimedSignalQueueSemaphore(
    uint32_t              deviceIdx,
    Queue*                pQueue,
    VkSemaphore           semaphore,
    uint64_t              value,
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (QueueSupportsTiming(deviceIdx, pQueue))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        result = m_trace.pGpaSession->TimedSignalQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);
#else
        result = m_trace.pGpaSession->TimedSignalQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo);
#endif
        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        result = pPalQueue->SignalQueueSemaphore(pQueueSemaphore, value);
#else
        result = pPalQueue->SignalQueueSemaphore(pQueueSemaphore);
#endif
    }

    return result;
}

// =====================================================================================================================
Pal::Result DevModeMgr::TimedWaitQueueSemaphore(
    uint32_t              deviceIdx,
    Queue*                pQueue,
    VkSemaphore           semaphore,
    uint64_t              value,
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (QueueSupportsTiming(deviceIdx, pQueue))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        result = m_trace.pGpaSession->TimedWaitQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);
#else
        result = m_trace.pGpaSession->TimedWaitQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo);
#endif
        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 458
        result = pPalQueue->WaitQueueSemaphore(pQueueSemaphore, value);
#else
        result = pPalQueue->WaitQueueSemaphore(pQueueSemaphore);
#endif
    }

    return result;
}

// =====================================================================================================================
Pal::Result DevModeMgr::TimedQueueSubmit(
    uint32_t                     deviceIdx,
    Queue*                       pQueue,
    uint32_t                     cmdBufferCount,
    const VkCommandBuffer*       pCommandBuffers,
    const Pal::SubmitInfo&       submitInfo,
    VirtualStackFrame*           pVirtStackFrame)
{
    VK_ASSERT(cmdBufferCount == submitInfo.cmdBufferCount);

    bool timingSupported = QueueSupportsTiming(deviceIdx, pQueue) && (submitInfo.cmdBufferCount > 0);

    // Fill in extra meta-data information to associate the API command buffer data with the generated
    // timing information.
    GpuUtil::TimedSubmitInfo timedSubmitInfo = {};
    Pal::uint64* pApiCmdBufIds  = nullptr;
    Pal::uint32* pSqttCmdBufIds = nullptr;

    if (timingSupported)
    {
        pApiCmdBufIds  = pVirtStackFrame->AllocArray<Pal::uint64>(cmdBufferCount);
        pSqttCmdBufIds = pVirtStackFrame->AllocArray<Pal::uint32>(cmdBufferCount);

        timedSubmitInfo.pApiCmdBufIds  = pApiCmdBufIds;
        timedSubmitInfo.pSqttCmdBufIds = pSqttCmdBufIds;
        timedSubmitInfo.frameIndex     = m_globalFrameIndex;

        timingSupported &= (pApiCmdBufIds != nullptr) && (pSqttCmdBufIds != nullptr);
    }

    Pal::Result result = Pal::Result::NotReady;

    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    if (timingSupported)
    {
        for (uint32_t cbIdx = 0; cbIdx < cmdBufferCount; ++cbIdx)
        {
            uintptr_t intHandle = reinterpret_cast<uintptr_t>(pCommandBuffers[cbIdx]);

            pApiCmdBufIds[cbIdx] = intHandle;

            CmdBuffer* pCmdBuf = ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[cbIdx]);

            pSqttCmdBufIds[cbIdx] = 0;

            if (pCmdBuf->GetSqttState() != nullptr)
            {
                pSqttCmdBufIds[cbIdx] = pCmdBuf->GetSqttState()->GetId().u32All;
            }

            VK_ASSERT(pCmdBuf->PalCmdBuffer(DefaultDeviceIndex) == submitInfo.ppCmdBuffers[cbIdx]);
        }

        // Do a timed submit of all the command buffers
        result = m_trace.pGpaSession->TimedSubmit(pPalQueue, submitInfo, timedSubmitInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    // Punt to non-timed submit if a timed submit fails (or is not supported)
    if (result != Pal::Result::Success)
    {
        result = pPalQueue->Submit(submitInfo);
    }

    if (pApiCmdBufIds != nullptr)
    {
        pVirtStackFrame->FreeArray(pApiCmdBufIds);
    }

    if (pSqttCmdBufIds != nullptr)
    {
        pVirtStackFrame->FreeArray(pSqttCmdBufIds);
    }

    return result;
}

// =====================================================================================================================
// Registers this pipeline, storing the code object binary and recording a load event in the RGP trace.
void DevModeMgr::PipelineCreated(
    Device*   pDevice,
    Pipeline* pPipeline)
{
    if ((m_trace.pDevice == pDevice) &&
        m_trace.pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (m_trace.pGpaSession != nullptr))
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 460
        m_trace.pGpaSession->RegisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex));
#else
        GpuUtil::RegisterPipelineInfo pipelineInfo = { 0 };
        pipelineInfo.apiPsoHash = pPipeline->GetApiHash();

        m_trace.pGpaSession->RegisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex), pipelineInfo);
#endif
    }
}

// =====================================================================================================================
// Unregisters this pipeline, recording an unload event in the RGP trace.
void DevModeMgr::PipelineDestroyed(
    Device*   pDevice,
    Pipeline* pPipeline)
{
    if ((m_trace.pDevice == pDevice) &&
        m_trace.pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (m_trace.pGpaSession != nullptr))
    {
        m_trace.pGpaSession->UnregisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex));
    }
}

#if VKI_GPUOPEN_PROTOCOL_ETW_CLIENT
Pal::Result DevModeMgr::InitEtwClient()
{
    // We should never have a valid etw client pointer already.
    VK_ASSERT(m_pEtwClient == nullptr);

    Pal::Result result = Pal::Result::Success;

    void* pStorage = m_pInstance->AllocMem(sizeof(DevDriver::ETWProtocol::ETWClient),
                                           VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

    // Attempt to create an ETW client for rgp traces.
    if (pStorage != nullptr)
    {
        m_pEtwClient = VK_PLACEMENT_NEW(pStorage)
            DevDriver::ETWProtocol::ETWClient(m_pDevDriverServer->GetMessageChannel());
    }
    else
    {
        result = Pal::Result::ErrorOutOfMemory;
    }

    // Attempt to locate an ETW server.
    DevDriver::ClientId etwProviderId = DevDriver::kBroadcastClientId;

    if (result == Pal::Result::Success)
    {
        DevDriver::ClientMetadata filter = {};
        filter.clientType = DevDriver::Component::Server;
        filter.protocols.etw = 1;

        result =
            DevDriverToPalResult(m_pDevDriverServer->GetMessageChannel()->FindFirstClient(filter, &etwProviderId));
    }

    // Connect to the server
    if (result == Pal::Result::Success)
    {
        result = DevDriverToPalResult(m_pEtwClient->Connect(etwProviderId));
    }

    if ((result != Pal::Result::Success) && (m_pEtwClient != nullptr))
    {
        Util::Destructor(m_pEtwClient);
        m_pInstance->FreeMem(m_pEtwClient);
        m_pEtwClient = nullptr;
    }

    return result;
}

void DevModeMgr::CleanupEtwClient()
{
    if (m_pEtwClient != nullptr)
    {
        m_pEtwClient->Disconnect();

        Util::Destructor(m_pEtwClient);
        m_pInstance->FreeMem(m_pEtwClient);
        m_pEtwClient = nullptr;
    }
}
#endif

}; // namespace vk

#endif
