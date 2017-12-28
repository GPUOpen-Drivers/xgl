/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace vk
{

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
DevModeMgr::DevModeMgr(Instance* pInstance)
    :
    m_pInstance(pInstance),
    m_pDevDriverServer(pInstance->PalPlatform()->GetDevDriverServer()),
    m_hardwareSupportsTracing(false),
    m_rgpServerSupportsTracing(false),
    m_finalized(false),
    m_tracingEnabled(false),
    m_numPrepFrames(0),
    m_traceGpuMemLimit(0),
    m_enableInstTracing(false),
    m_enableSampleUpdates(false),
    m_globalFrameIndex(1) // Must start from 1 according to RGP spec
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

    // Tell RGP that the server (i.e. the driver) supports tracing if requested.
    if (result == Pal::Result::Success)
    {
        DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

        if (pRGPServer != nullptr)
        {
            m_rgpServerSupportsTracing = (pRGPServer->EnableTraces() == DevDriver::Result::Success);
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
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if ((pRGPServer != nullptr) && (m_hardwareSupportsTracing == false))
    {
        pRGPServer->DisableTraces();
    }

    // Finalize the devmode manager
    m_pDevDriverServer->Finalize();

    // Figure out if tracing support should be enabled or not
    m_finalized      = true;
    m_tracingEnabled = (pRGPServer != nullptr) && pRGPServer->TracesEnabled();
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
// Called before a swap chain presents.  This signals a frame-end boundary and is used to coordinate RGP trace
// start/stop.
void DevModeMgr::PrePresent(const Queue* pQueue)
{
    // Get the RGP message server
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if ((pRGPServer != nullptr) && pRGPServer->TracesEnabled())
    {
        Device* pPresentDevice = pQueue->VkDevice();

        // Only act if this present is coming from the same device that started the trace
        if (m_trace.pDevice == pPresentDevice)
        {
            // If there's currently a trace running, submit the trace-end command buffer
            if (m_trace.status == TraceStatus::Running)
            {
                Util::MutexAuto traceLock(&m_traceMutex);

                if (m_trace.pDevice == pPresentDevice) // Double-checked lock
                {
                    m_trace.sqttFrameCount++;

                    if (m_trace.sqttFrameCount >= m_trace.pDevice->GetRuntimeSettings().devModeSqttFrameCount)
                    {
                        if (EndRGPHardwareTrace(&m_trace, pQueue) != Pal::Result::Success)
                        {
                            FinishRGPTrace(&m_trace, true);
                        }
                    }
                }
            }

            if (IsQueueTimingActive(pPresentDevice))
            {
                // Call TimedQueuePresent() to insert commands that collect GPU timestamp.
                Pal::IQueue* pPalQueue = pQueue->PalQueue();

                // Currently nothing in the PresentInfo struct is used for inserting a timed present marker.
                GpuUtil::TimedQueuePresentInfo timedPresentInfo = {};
                Pal::Result result = m_trace.pGpaSession->TimedQueuePresent(pPalQueue, timedPresentInfo);
                VK_ASSERT(result == Pal::Result::Success);
            }
        }
    }
}

// =====================================================================================================================
Pal::Result DevModeMgr::CheckForTraceResults(TraceState* pState)
{
    // Get the RGP message server
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    VK_ASSERT(pState->status == TraceStatus::WaitingForResults);

    Pal::Result result = Pal::Result::NotReady;

    // Check if trace results are ready
    if (pState->pGpaSession->IsReady()                              && // GPA session is ready
        (pState->pBeginFence->GetStatus() != Pal::Result::NotReady) && // "Trace begin" cmdbuf has retired
        (pState->pEndFence->GetStatus()   != Pal::Result::NotReady))   // "Trace end" cmdbuf has retired
    {
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
                auto devResult = pRGPServer->WriteTraceData(static_cast<Pal::uint8*>(pTraceData), traceDataSize);

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
// Called after a swap chain presents.  This signals a (next) frame-begin boundary and is used to coordinate
// RGP trace start/stop.
void DevModeMgr::PostPresent(const Queue* pQueue)
{
    // Wait for the driver to be resumed in case it's been paused.
    WaitForDriverResume();

    // Get the RGP message server
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if ((pRGPServer != nullptr) && pRGPServer->TracesEnabled())
    {
        Util::MutexAuto traceLock(&m_traceMutex);

        // Check if there's an RGP trace request pending and we're idle
        if ((m_trace.status == TraceStatus::Idle) && pRGPServer->IsTracePending())
        {
            // Attempt to start preparing for a trace
            if (PrepareRGPTrace(&m_trace, pQueue) == Pal::Result::Success)
            {
                // Attempt to start the trace immediately if we do not need to prepare
                if (m_numPrepFrames == 0)
                {
                    if (BeginRGPTrace(&m_trace, pQueue) != Pal::Result::Success)
                    {
                        FinishRGPTrace(&m_trace, true);
                    }
                }
            }
        }
        else if (m_trace.status == TraceStatus::Preparing)
        {
            // Wait some number of "preparation frames" before starting the trace in order to get enough
            // timer samples to sync CPU/GPU clock domains.
            m_trace.preparedFrameCount++;

            // Take a calibration timing measurement sample for this frame.
            m_trace.pGpaSession->SampleTimingClocks();

            // Start the SQTT trace if we've waited a sufficient number of preparation frames
            if (m_trace.preparedFrameCount >= m_numPrepFrames)
            {
                Pal::Result result = BeginRGPTrace(&m_trace, pQueue);

                if (result != Pal::Result::Success)
                {
                    FinishRGPTrace(&m_trace, true);
                }
            }
            else if ((m_trace.preparedFrameCount == (m_numPrepFrames - 1)) &&
                     m_enableSampleUpdates                                 &&
                     m_trace.flushAllQueues)
            {
                // Flush all queues on the last preparation frame.
                // We only need this if mid-trace sample updates are enabled and the driver setting for flushing queues
                // is also enabled. This is used to provide RGP with a guaranteed idle point in the thread trace data.
                // That point can be used to synchronize the hardware pipeline stages in the sqtt parsing logic.
                Pal::Result result = Pal::Result::Success;

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
                                submitInfo.ppCmdBuffers = &pFamilyState->pTraceFlushCmdBuf;
                                submitInfo.pFence = nullptr;

                                result = pQueueState->pQueue->PalQueue()->Submit(submitInfo);

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

                if (result != Pal::Result::Success)
                {
                    FinishRGPTrace(&m_trace, true);
                }
            }
        }
        // Check if we're ending a trace waiting for SQTT to turn off.  If SQTT has turned off, end the trace
        else if (m_trace.status == TraceStatus::WaitingForSqtt)
        {
            Pal::Result fenceResult = m_trace.pEndSqttFence->GetStatus();
            Pal::Result result      = Pal::Result::Success;

            if (fenceResult == Pal::Result::Success)
            {
                result = EndRGPTrace(&m_trace, pQueue);
            }
            else if (fenceResult != Pal::Result::NotReady)
            {
                result = fenceResult;
            }

            if (result != Pal::Result::Success)
            {
                FinishRGPTrace(&m_trace, true);
            }
        }
        // Check if we're waiting for final trace results.
        else if (m_trace.status == TraceStatus::WaitingForResults)
        {
            Pal::Result result = CheckForTraceResults(&m_trace);

            // Results ready: finish trace
            if (result == Pal::Result::Success)
            {
                FinishRGPTrace(&m_trace, false);
            }
            // Error while computing results: abort trace
            else if (result != Pal::Result::NotReady)
            {
                FinishRGPTrace(&m_trace, true);
            }
        }
    }

    m_globalFrameIndex++;
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
// This function starts preparing for an RGP trace.  Preparation involves some N frames of lead-up time during which
// timing samples are accumulated to synchronize CPU and GPU clock domains.
//
// This function transitions from the Idle state to the Preparing state.
Pal::Result DevModeMgr::PrepareRGPTrace(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::Idle);

    // We can only trace using a single device at a time currently, so recreate RGP trace
    // resources against this new one if the device is changing.
    Pal::Result result = CheckTraceDeviceChanged(pState, pQueue->VkDevice());
    Device* pDevice = pState->pDevice;

    // Update our trace parameters based on the new trace
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if (pRGPServer != nullptr)
    {
        const auto traceParameters = pRGPServer->QueryTraceParameters();

        m_numPrepFrames        = traceParameters.numPreparationFrames;
        m_traceGpuMemLimit     = traceParameters.gpuMemoryLimitInMb * 1024 * 1024;
        m_enableInstTracing    = traceParameters.flags.enableInstructionTokens;

        // If we're presenting from a compute queue and the trace parameters indicate that we want to support
        // compute queue presents, then we need to enable sample updates for this trace. Mid-trace sample updates
        // allow us to capture a smaller set of trace data as the preparation frames run, then change the sqtt
        // token mask before the last frame to capture the full token set. RGP requires the additional data
        // from this technique in order to handle edge cases surrounding compute queue presentation.
        m_enableSampleUpdates = ((pQueue->PalQueue()->Type() == Pal::QueueTypeCompute) &&
                                 traceParameters.flags.allowComputePresents);
    }
    else
    {
        result = Pal::Result::ErrorIncompatibleDevice;
    }

    // Notify the RGP server that we are starting a trace
    if (result == Pal::Result::Success)
    {
        if (pRGPServer->BeginTrace() != DevDriver::Result::Success)
        {
            result = Pal::Result::ErrorUnknown;
        }
    }

    // Tell the GPA session class we're starting a trace
    if (result == Pal::Result::Success)
    {
        GpuUtil::GpaSessionBeginInfo info = {};

        info.flags.enableQueueTiming   = pState->queueTimingEnabled;
        info.flags.enableSampleUpdates = m_enableSampleUpdates;

        result = pState->pGpaSession->Begin(info);
    }

    pState->preparedFrameCount = 0;
    pState->sqttFrameCount     = 0;

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
        sampleConfig.sqtt.gpuMemoryLimit                 = m_traceGpuMemLimit;
        sampleConfig.sqtt.flags.enable                   = true;
        sampleConfig.sqtt.flags.supressInstructionTokens = (m_enableInstTracing == false);

        // Override trace buffer size from panel
        if (pDevice->GetRuntimeSettings().devModeSqttGpuMemoryLimit != 0)
        {
            sampleConfig.sqtt.gpuMemoryLimit = pDevice->GetRuntimeSettings().devModeSqttGpuMemoryLimit;
        }

        pState->gpaSampleId = pState->pGpaSession->BeginSample(pBeginCmdBuf, sampleConfig);
    }

    // Finish building the trace-begin command buffer
    if (result == Pal::Result::Success)
    {
        result = pBeginCmdBuf->End();
    }

    // Reset the trace-begin fence
    if (result == Pal::Result::Success)
    {
        // Register this as an active command buffer
        VK_ASSERT(pState->activeCmdBufCount < VK_ARRAY_SIZE(pState->pActiveCmdBufs));

        pState->pActiveCmdBufs[pState->activeCmdBufCount++] = pBeginCmdBuf;

        result = pDevice->PalDevice()->ResetFences(1, &pState->pBeginFence);
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
            pState->pGpaSession->UpdateSampleTraceParams(pBeginSqttCmdBuf, pState->gpaSampleId);
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

            result = pQueue->PalQueue()->Submit(submitInfo);
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
    }
    else
    {
        // We failed to prepare for the trace so abort it.
        if (pRGPServer != nullptr)
        {
            const DevDriver::Result devDriverResult = pRGPServer->AbortTrace();

            // AbortTrace should always succeed unless we've used the api incorrectly.
            VK_ASSERT(devDriverResult == DevDriver::Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// This function begins an RGP trace by initializing all dependent resources and submitting the "begin trace"
// information command buffer.
//
// This function transitions from the Preparing state to the Running state.
Pal::Result DevModeMgr::BeginRGPTrace(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(m_trace.status == TraceStatus::Preparing);
    VK_ASSERT(m_tracingEnabled);

    // We can only trace using a single device at a time currently, so recreate RGP trace
    // resources against this new one if the device is changing.
    Pal::Result result = CheckTraceDeviceChanged(pState, pQueue->VkDevice());

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

        result = pQueue->PalQueue()->Submit(submitInfo);
    }

    // Make the trace active and remember which queue started it
    if (result == Pal::Result::Success)
    {
        pState->status           = TraceStatus::Running;
        pState->pTraceBeginQueue = pTraceQueue;
    }

    return result;
}

// =====================================================================================================================
// This function submits the command buffer to stop SQTT tracing.  Full tracing still continues.
//
// This function transitions from the Running state to the WaitingForSqtt state.
Pal::Result DevModeMgr::EndRGPHardwareTrace(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::Running);

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
        pPalDevice     = pState->pDevice->PalDevice();
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

        result = pQueue->PalQueue()->Submit(submitInfo);
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
Pal::Result DevModeMgr::EndRGPTrace(
    TraceState*  pState,
    const Queue* pQueue)
{
    VK_ASSERT(pState->status == TraceStatus::WaitingForSqtt);

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
        pPalDevice = pState->pDevice->PalDevice();
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

        result = pQueue->PalQueue()->Submit(submitInfo);
    }

    if (result == Pal::Result::Success)
    {
        pState->status         = TraceStatus::WaitingForResults;
        pState->pTraceEndQueue = pTraceQueue;
    }

    return result;
}

// =====================================================================================================================
// This function resets and possibly cancels a currently active (between begin/end) RGP trace.  It frees any dependent
// resources.
void DevModeMgr::FinishRGPTrace(
    TraceState* pState,
    bool        aborted)
{
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    VK_ASSERT(pRGPServer != nullptr);

    // Inform RGP protocol that we're done with the trace, either by aborting it or finishing normally
    if (aborted)
    {
        pRGPServer->AbortTrace();
    }
    else
    {
        pRGPServer->EndTrace();
    }

    if (pState->pGpaSession != nullptr)
    {
        pState->pGpaSession->Reset();
    }

    // Reset tracing state to idle
    pState->preparedFrameCount = 0;
    pState->sqttFrameCount     = 0;
    pState->gpaSampleId        = 0;
    pState->status             = TraceStatus::Idle;
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
        // If we are idle, we can re-initialize trace resources based on the new device.
        if (pState->status == TraceStatus::Idle)
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
    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if (pState->status != TraceStatus::Idle)
    {
        FinishRGPTrace(pState, true);
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
                                                            pState->pDevice->VkPhysicalDevice(), familyIdx);
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
                        Pal::Result queryKernelSuccess = pQueue->PalQueue()->QueryKernelContextInfo(&kernelContextInfo);

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
        Pal::IDevice* pPalDevice = pTraceState->pDevice->PalDevice();

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

    DevDriver::RGPProtocol::RGPServer* pRGPServer = m_pDevDriverServer->GetRGPServer();

    if ((m_tracingEnabled == false) ||  // Tracing is globally disabled
        (pRGPServer == nullptr) ||      // There is no RGP server (this should never happen)
        (pDevice->NumPalDevices() > 1)) // MGPU device group tracing is not currently supported
    {
        result = Pal::Result::ErrorInitializationFailed;
    }

    // Fail initialization of trace resources if SQTT tracing has been force-disabled from the panel (this will
    // consequently fail the trace), or if the chosen device's gfxip does not support SQTT.
    //
    // It's necessary to check this during RGP tracing init in addition to devmode init because during the earlier
    // devmode init we may be in a situation where some enumerated physical devices support tracing and others do not.
    if (GpuSupportsTracing(pDevice->VkPhysicalDevice()->PalProperties(), pDevice->GetRuntimeSettings()) == false)
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

    Pal::IDevice* pPalDevice = pDevice->PalDevice();

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

    if (result != Pal::Result::Success)
    {
        // If we've failed to initialize tracing, permanently disable traces
        if (pRGPServer != nullptr)
        {
            pRGPServer->DisableTraces();

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
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (QueueSupportsTiming(deviceIdx, pQueue))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;

        result = m_trace.pGpaSession->TimedSignalQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->SignalQueueSemaphore(pQueueSemaphore);
    }

    return result;
}

// =====================================================================================================================
Pal::Result DevModeMgr::TimedWaitQueueSemaphore(
    uint32_t              deviceIdx,
    Queue*                pQueue,
    VkSemaphore           semaphore,
    Pal::IQueueSemaphore* pQueueSemaphore)
{
    Pal::IQueue* pPalQueue = pQueue->PalQueue(deviceIdx);

    Pal::Result result = Pal::Result::NotReady;

    if (QueueSupportsTiming(deviceIdx, pQueue))
    {
        GpuUtil::TimedQueueSemaphoreInfo timedSemaphoreInfo = {};

        timedSemaphoreInfo.semaphoreID = (uint64_t)semaphore;

        result = m_trace.pGpaSession->TimedWaitQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->WaitQueueSemaphore(pQueueSemaphore);
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

            VK_ASSERT(pCmdBuf->PalCmdBuffer() == submitInfo.ppCmdBuffers[cbIdx]);
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
// Registers this pipeline as being included in the RGP trace.  Stores shader code binary etc.
void DevModeMgr::PipelineCreated(
    Device*   pDevice,
    Pipeline* pPipeline)
{
    if ((m_trace.pDevice == pDevice) &&
        m_trace.pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (m_trace.pGpaSession != nullptr))
    {
        m_trace.pGpaSession->RegisterPipeline(pPipeline->PalPipeline());
    }
}

}; // namespace vk

#endif

