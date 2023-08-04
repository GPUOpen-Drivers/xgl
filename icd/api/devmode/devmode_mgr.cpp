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
 * @file  devmode_mgr.cpp
 * @brief Contains implementation of the GPU Open Developer Mode manager
 ***********************************************************************************************************************
 */

#if ICD_GPUOPEN_DEVMODE_BUILD
// Vulkan headers
#include "devmode/devmode_mgr.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_instance.h"
#include "include/vk_pipeline.h"
#include "include/vk_physical_device.h"
#include "include/vk_utils.h"
#include "include/vk_conv.h"
#include "include/pipeline_binary_cache.h"
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

// PAL headers
#include "pal.h"
#include "palCmdAllocator.h"
#include "palFence.h"
#include "palQueueSemaphore.h"
#include "palHashBaseImpl.h"
#include "palListImpl.h"
#include "palVectorImpl.h"

// gpuopen headers
#include "devDriverServer.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/rgpServer.h"
#include "protocols/driverControlServer.h"
#include "protocols/ddPipelineUriService.h"
#include "protocols/ddEventServer.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_ray_tracing_pipeline.h"
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
// Callback method for providing hashes and sizes for tracked pipelines to the PipelineUriService
static DevDriver::Result GetPipelineHashes(
    DevDriver::PipelineUriService* pService,
    void*                          pUserData,
    DevDriver::ExclusionFlags      /*flags*/)
{
    DevModeMgr* pDevModeMgr = static_cast<DevModeMgr*>(pUserData);

    DevDriver::Result result = DevDriver::Result::NotReady;

    Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> cacheListLock(pDevModeMgr->GetPipelineReinjectionLock());

    auto                 pipelineCacheIter = pDevModeMgr->GetPipelineCacheListIterator();

    while (pipelineCacheIter.Get() != nullptr)
    {
        result = DevDriver::Result::Success;

        PipelineBinaryCache* pPipelineCache = *pipelineCacheIter.Get();

        Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> hashMappingLock(pPipelineCache->GetHashMappingLock());

        auto hashMappingIter   = pPipelineCache->GetHashMappingIterator();

        while (hashMappingIter.Get() != nullptr)
        {
            const Pal::PipelineHash&            internalPipelineHash = hashMappingIter.Get()->key;
            const PipelineBinaryCache::CacheId& cacheId              = hashMappingIter.Get()->value;

            Util::QueryResult query = {};

            // Do not throw an error if entry is not found in cache (in case it was evicted)
            if (pPipelineCache->QueryPipelineBinary(&cacheId, 0, &query) == Util::Result::Success)
            {
                pService->AddHash(internalPipelineHash, query.dataSize);
            }

            hashMappingIter.Next();
        }

        pipelineCacheIter.Next();
    }

    return result;
}

// =====================================================================================================================
// Callback method for providing binaries for tracked pipelines to the PipelineUriService
static DevDriver::Result GetPipelineCodeObjects(
    DevDriver::PipelineUriService* pService,
    void*                          pUserData,
    DevDriver::ExclusionFlags      /*flags*/,
    const DevDriver::PipelineHash* pPipelineHashes,
    size_t                         numHashes)
{
    DevModeMgr* pDevModeMgr = static_cast<DevModeMgr*>(pUserData);

    DevDriver::Result result = DevDriver::Result::NotReady;

    Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> cacheListLock(pDevModeMgr->GetPipelineReinjectionLock());

    auto                 pipelineCacheIter = pDevModeMgr->GetPipelineCacheListIterator();

    while (pipelineCacheIter.Get() != nullptr)
    {
        result = DevDriver::Result::Success;

        PipelineBinaryCache* pPipelineCache = *pipelineCacheIter.Get();

        if (pPipelineHashes != nullptr)
        {
            // A specific list of hashes were requested
            for (uint32_t i = 0; i < numHashes; i += 1)
            {
                DevDriver::PipelineRecord record = {};
                record.header.hash = pPipelineHashes[i];

                size_t            binarySize = 0u;
                const void*       pBinary = nullptr;

                static_assert(sizeof(Pal::PipelineHash) == sizeof(record.header.hash), "Structure size mismatch");

                PipelineBinaryCache::CacheId* pCacheId =
                    pPipelineCache->GetCacheIdForPipeline(reinterpret_cast<Pal::PipelineHash*>(&record.header.hash));

                if ((pCacheId != nullptr) &&
                    (pPipelineCache->LoadPipelineBinary(pCacheId, &binarySize, &pBinary) == Util::Result::Success))
                {
                    record.pBinary = pBinary;
                    record.header.size = binarySize;
                }

                // Empty record is written if hash is not found
                pService->AddPipeline(record);
            }
        }
        else
        {
            Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> hashMappingLock(pPipelineCache->GetHashMappingLock());

            auto hashMappingIter   = pPipelineCache->GetHashMappingIterator();

            while (hashMappingIter.Get() != nullptr)
            {
                Pal::PipelineHash&            internalPipelineHash = hashMappingIter.Get()->key;
                PipelineBinaryCache::CacheId& cacheId              = hashMappingIter.Get()->value;

                size_t binarySize = 0u;
                const void* pBinary = nullptr;

                if (pPipelineCache->LoadPipelineBinary(&cacheId, &binarySize, &pBinary) == Util::Result::Success)
                {
                    DevDriver::PipelineRecord record = {};
                    record.pBinary = pBinary;
                    record.header.size = binarySize;
                    record.header.hash = DevDriver::PipelineHash{ internalPipelineHash };

                    pService->AddPipeline(record);
                }

                hashMappingIter.Next();
            }
        }

        pipelineCacheIter.Next();
    }

    return result;
}

// =====================================================================================================================
// Callback method for reinjecting binaries back into the cache
static DevDriver::Result InjectPipelineCodeObjects(
    void*                               pUserData,
    DevDriver::PipelineRecordsIterator& pipelineIter)
{
    DevModeMgr* pDevModeMgr = static_cast<DevModeMgr*>(pUserData);

    DevDriver::Result result = DevDriver::Result::NotReady;

    uint32_t replacedCount = 0u;
    DevDriver::PipelineRecord record;

    Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> cacheListLock(pDevModeMgr->GetPipelineReinjectionLock());

    auto pipelineCacheIter = pDevModeMgr->GetPipelineCacheListIterator();

    while (pipelineCacheIter.Get() != nullptr)
    {
        result = DevDriver::Result::Success;

        PipelineBinaryCache* pPipelineCache = *pipelineCacheIter.Get();

        while (pipelineIter.Get(&record))
        {
            static_assert(sizeof(PipelineBinaryCache::CacheId) == sizeof(record.header.hash), "Structure size mismatch");

            size_t                              binarySize            = static_cast<size_t>(record.header.size);
            const PipelineBinaryCache::CacheId* pInternalPipelineHash =
                reinterpret_cast<PipelineBinaryCache::CacheId*>(&record.header.hash);

            if (pPipelineCache->StoreReinjectionBinary(pInternalPipelineHash, binarySize, record.pBinary) == Util::Result::Success)
            {
                replacedCount++;
            }

            pipelineIter.Next();
        }

        pipelineCacheIter.Next();
    }

    if ((result == DevDriver::Result::Success) &&
        (replacedCount == 0u))
    {
        result = DevDriver::Result::Error;
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
    m_finalized(false),
    m_triggerMode(TriggerMode::Present),
    m_numPrepFrames(0),
    m_traceGpuMemLimit(0),
    m_enableInstTracing(false),
    m_enableSampleUpdates(false),
    m_allowComputePresents(false),
    m_blockingTraceEnd(false),
    m_globalFrameIndex(1), // Must start from 1 according to RGP spec
    m_traceFrameBeginTag(0),
    m_traceFrameEndTag(0),
    m_targetApiPsoHash(0),
    m_seMask(0),
    m_perfCountersEnabled(false),
    m_perfCounterMemLimit(0),
    m_perfCounterFrequency(0),
    m_useStaticVmid(false),
    m_staticVmidActive(false),
    m_perfCounterIds(pInstance->Allocator()),
    m_pipelineCaches(pInstance->Allocator()),
    m_crashAnalysisEnabled(false)
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
    Pal::Result result = Pal::Result::Success;

    if (m_pDevDriverServer != nullptr)
    {
        m_pRGPServer = m_pDevDriverServer->GetRGPServer();
    }

    return result;
}

// =====================================================================================================================
// Called during initial device enumeration prior to calling Pal::IDevice::CommitSettingsAndInit().
//
// This finalizes the developer driver manager.
void DevModeMgr::Finalize(
    uint32_t              deviceCount,
    VulkanSettingsLoader* settingsLoaders[])
{
    if (m_pRGPServer != nullptr)
    {
        bool tracingForceDisabledForAllGpus = true;

        for (uint32_t gpu = 0; gpu < deviceCount; ++gpu)
        {
            if (settingsLoaders[gpu]->GetSettings().devModeSqttForceDisable == false)
            {
                tracingForceDisabledForAllGpus = false;

                break;
            }
        }

        // If tracing is force disabled for all GPUs, inform the RGP server to disable tracing
        if (tracingForceDisabledForAllGpus)
        {
            m_pRGPServer->DisableTraces();
        }
    }

    m_pDevDriverServer->GetDriverControlServer()->StartLateDeviceInit();

    // Finalize the devmode manager
    m_pDevDriverServer->Finalize();

    m_crashAnalysisEnabled = m_pInstance->PalPlatform()->IsCrashAnalysisModeEnabled();

    m_finalized      = true;
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
    pDriverControlServer->DriverTick();
}

// =====================================================================================================================
// Called to notify of a frame-end boundary and is used to coordinate RGP trace start/stop.
//
// "delimiterType" represents how the transition/notify was triggered.
void DevModeMgr::NotifyFrameEnd(
    const Queue*       pQueue,
    FrameDelimiterType delimiterType)
{
    // Get the RGP message server
    if (IsTracingEnabled())
    {
        // Don't act if a QueuePresent is coming, but a QueueLabel was previously seen
        if ((delimiterType != FrameDelimiterType::QueuePresent) ||
            (m_trace.labelDelimsPresent == false))
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

                    AdvanceActiveTraceStep(&m_trace, pQueue, false, delimiterType);
                }
            }
        }
    }

    m_globalFrameIndex++;
}

// =====================================================================================================================
void DevModeMgr::AdvanceActiveTraceStep(
    TraceState*        pState,
    const Queue*       pQueue,
    bool               beginFrame,
    FrameDelimiterType delimiterType)
{
    // In present trigger mode, we should advance when a real (or dummy) present occurs.
    // In index trigger mode, we should advance when a specific present index occurs.
    // In tag trigger mode, we should advance when a tag trigger is encountered.
    constexpr uint32_t delimiterToValidTriggers[static_cast<uint32_t>(FrameDelimiterType::Count)] =
    {
        (1 << static_cast<uint32_t>(TriggerMode::Present)) |
            (1 << static_cast<uint32_t>(TriggerMode::Index)), // Frame::Delimiter::QueuePresent
        (1 << static_cast<uint32_t>(TriggerMode::Present)) |
            (1 << static_cast<uint32_t>(TriggerMode::Index)), // Frame::Delimiter::QueueLabel
        (1 << static_cast<uint32_t>(TriggerMode::Tag))        // Frame::Delimiter::CmdBufferTag
    };

    VK_ASSERT(pState->status != TraceStatus::Idle);

    // Only advance the trace step if we're processing the right type of trigger.ac
    if ((delimiterToValidTriggers[static_cast<uint32_t>(delimiterType)] &
            (1 << static_cast<uint32_t>(m_triggerMode))) != 0)
    {
        if (m_trace.status == TraceStatus::Pending)
        {
            // Attempt to start preparing for a trace
            if (TracePendingToPreparingStep(&m_trace, pQueue, delimiterType) != Pal::Result::Success)
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
                const RuntimeSettings& settings = pState->pDevice->GetRuntimeSettings();

                if (settings.devModeEnableRgpTraceDump)
                {
                    Util::File dumpFile;
                    if (dumpFile.Open(settings.devModeRgpTraceDumpFile, Util::FileAccessMode::FileAccessWrite | Util::FileAccessMode::FileAccessBinary) == Util::Result::Success)
                    {
                        dumpFile.Write(pTraceData, traceDataSize);
                        dumpFile.Close();
                    }
                    else
                    {
                        VK_ALERT_ALWAYS_MSG("Failed to open RGP trace dump file: %s", settings.devModeRgpTraceDumpFile);
                    }
                }

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
// "delimiterType" represents how the transition/notify was triggered.
void DevModeMgr::NotifyFrameBegin(
    const Queue*       pQueue,
    FrameDelimiterType delimiterType)
{
    // Wait for the driver to be resumed in case it's been paused.
    WaitForDriverResume();

    if (IsTracingEnabled())
    {
        // Don't act if a QueuePresent is coming, but a QueueLabel was previously seen
        if ((delimiterType != FrameDelimiterType::QueuePresent) ||
            (m_trace.labelDelimsPresent == false))
        {
            if (delimiterType == FrameDelimiterType::QueueLabel)
            {
                m_trace.labelDelimsPresent = true;
            }

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
                    AdvanceActiveTraceStep(&m_trace, pQueue, true, delimiterType);
                }
            }
        }
    }
}

// =====================================================================================================================
// Returns the queue state for this aparticular queue.
DevModeMgr::TraceQueueState* DevModeMgr::FindTraceQueueState(
    TraceState*  pState,
    const Queue* pQueue)
{
    TraceQueueState* pTraceQueue = nullptr;

    for (uint32_t queue = 0; (queue < pState->queueCount) && (pTraceQueue == nullptr); queue++)
    {
        if (pState->queueState[queue].pQueue == pQueue)
        {
            pTraceQueue = &pState->queueState[queue];
        }
    }

    if (pTraceQueue == nullptr)
    {
        for (uint32_t queue = 0; (queue < pState->auxQueueCount) && (pTraceQueue == nullptr); queue++)
        {
            if (pState->auxQueueStates[queue].pQueue == pQueue)
            {
                pTraceQueue = &pState->auxQueueStates[queue];
            }
        }

        if (pTraceQueue == nullptr)
        {
            if (InitTraceQueueResources(pState, nullptr, pQueue, true) == Pal::Result::Success)
            {
                pTraceQueue = &pState->auxQueueStates[pState->auxQueueCount - 1];
            }
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
            // Override some parameters via panel prior to updating trace parameters
            const RuntimeSettings& settings = pState->pDevice->GetRuntimeSettings();

            // Update our trace parameters based on the new trace
            const auto traceParameters = m_pRGPServer->QueryTraceParameters();

            m_numPrepFrames        = settings.devModeSqttPrepareFrameCount != UINT_MAX ?
                settings.devModeSqttPrepareFrameCount : traceParameters.numPreparationFrames;
            m_traceGpuMemLimit     = traceParameters.gpuMemoryLimitInMb * 1024 * 1024;
            m_enableInstTracing    = traceParameters.flags.enableInstructionTokens;
            m_allowComputePresents = traceParameters.flags.allowComputePresents;
            m_seMask               = traceParameters.seMask;

            m_perfCountersEnabled  = (traceParameters.flags.enableSpm != 0);

            DevDriver::RGPProtocol::ServerSpmConfig counterConfig = {};
            DevDriver::Vector<DevDriver::RGPProtocol::ServerSpmCounterId>
                counters(m_pDevDriverServer->GetMessageChannel()->GetAllocCb());
            m_pRGPServer->QuerySpmConfig(&counterConfig, &counters);

            Pal::PerfExperimentProperties perfProperties = {};

            const Pal::Result palResult =
                pState->pDevice->PalDevice(DefaultDeviceIndex)->GetPerfExperimentProperties(&perfProperties);

            // Querying performance properties should never fail
            VK_ASSERT(palResult == Pal::Result::Success);

            m_perfCounterFrequency = counterConfig.sampleFrequency;
            m_perfCounterMemLimit  = counterConfig.memoryLimitInMb * 1024 * 1024;

            m_perfCounterIds.Clear();

            for (size_t counterIndex = 0; counterIndex < counters.Size(); ++counterIndex)
            {
                const DevDriver::RGPProtocol::ServerSpmCounterId serverCounter = counters[counterIndex];
                const Pal::GpuBlockPerfProperties& blockPerfProps = perfProperties.blocks[serverCounter.blockId];

                if (serverCounter.instanceId == DevDriver::RGPProtocol::kSpmAllInstancesId)
                {
                    for (uint32 instanceIndex = 0; instanceIndex < blockPerfProps.instanceCount; ++instanceIndex)
                    {
                        GpuUtil::PerfCounterId counterId = {};
                        counterId.block = static_cast<Pal::GpuBlock>(serverCounter.blockId);
                        counterId.instance = instanceIndex;
                        counterId.eventId = serverCounter.eventId;

                        m_perfCounterIds.PushBack(counterId);
                    }
                }
                else
                {
                    GpuUtil::PerfCounterId counterId = {};
                    counterId.block = static_cast<Pal::GpuBlock>(serverCounter.blockId);
                    counterId.instance = serverCounter.instanceId;
                    counterId.eventId = serverCounter.eventId;

                    m_perfCounterIds.PushBack(counterId);
                }
            }

            // Initially assume we don't need to block on trace end.  This may change during transition to
            // Preparing.
            m_blockingTraceEnd = false;

            // Store the targtet API PSO hash to be passed to GpaSession::SetSampleTraceApiInfo
            m_targetApiPsoHash = traceParameters.pipelineHash;

            if (traceParameters.captureMode == DevDriver::RGPProtocol::CaptureTriggerMode::Index)
            {
                m_triggerMode = TriggerMode::Index;

                m_traceFrameBeginIndex = traceParameters.captureStartIndex;
                m_traceFrameEndIndex   = traceParameters.captureStopIndex;

                m_traceFrameBeginTag = 0;
                m_traceFrameEndTag = 0;

                if (m_traceFrameBeginIndex < m_numPrepFrames)
                {
                    VK_NEVER_CALLED();
                    FinishOrAbortTrace(pState, true);
                }
            }
            else if (traceParameters.captureMode == DevDriver::RGPProtocol::CaptureTriggerMode::Markers)
            {
                m_triggerMode = TriggerMode::Tag;

                m_traceFrameBeginTag = traceParameters.beginTag;
                m_traceFrameEndTag   = traceParameters.endTag;

                VK_ASSERT((m_traceFrameBeginTag != 0) || (m_traceFrameEndTag != 0));
            }
            else if (traceParameters.captureMode == DevDriver::RGPProtocol::CaptureTriggerMode::Present)
            {
                m_triggerMode = TriggerMode::Present;

                m_traceFrameBeginTag = 0;
                m_traceFrameEndTag = 0;
            }
            else
            {
                m_triggerMode = TriggerMode::Present;

                VK_NOT_IMPLEMENTED;
            }

            // Override some parameters via panel after updating trace parameters
            if (settings.devModeSqttTraceBeginEndTagEnable)
            {
                m_traceFrameBeginTag = settings.devModeSqttTraceBeginTagValue;
                m_traceFrameEndTag   = settings.devModeSqttTraceEndTagValue;
            }

            // Reset trace device status
            pState->preparedFrameCount = 0;
            pState->sqttFrameCount     = 0;
            pState->status             = TraceStatus::Pending;
        }
    }
}

// =====================================================================================================================
// This function starts preparing for an RGP trace.  Preparation involves some N frames of lead-up time during which
// timing samples are accumulated to synchronize CPU and GPU clock domains.
//
// "delimiterType" represents how the transition/notify was triggered.
//
// This function transitions from the Pending state to the Preparing state.
Pal::Result DevModeMgr::TracePendingToPreparingStep(
    TraceState*        pState,
    const Queue*       pQueue,
    FrameDelimiterType delimiterType)
{
    VK_ASSERT(pState->status  == TraceStatus::Pending);

    // We need to hold off untill we reach the desired frame when in Index mode
    if ((m_triggerMode == TriggerMode::Index) &&
        (m_globalFrameIndex < (m_traceFrameBeginIndex - m_numPrepFrames)))
    {
        return Pal::Result::Success;
    }

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

    // Activate static VMID if supported
    if (result == Pal::Result::Success)
    {
        VK_ASSERT(m_staticVmidActive == false);

        if (m_useStaticVmid)
        {
            result = pDevice->PalDevice(DefaultDeviceIndex)->SetStaticVmidMode(true);

            m_staticVmidActive = (result == Pal::Result::Success);
        }
    }

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
        info.flags.useInternalQueueSemaphoreTiming = true;

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

        // Configure SQTT
        sampleConfig.sqtt.seDetailedMask                 = m_seMask;
        sampleConfig.sqtt.gpuMemoryLimit                 = (settings.devModeSqttGpuMemoryLimit == 0) ?
                                                               m_traceGpuMemLimit : settings.devModeSqttGpuMemoryLimit;
        sampleConfig.sqtt.flags.enable                   = true;
        sampleConfig.sqtt.flags.supressInstructionTokens = (m_enableInstTracing == false) || (m_targetApiPsoHash != 0);
        sampleConfig.sqtt.flags.stallMode                = Pal::GpuProfilerStallMode::GpuProfilerStallAlways;

        // Configure SPM
        if (m_perfCountersEnabled && (m_perfCounterIds.IsEmpty() == false))
        {
            sampleConfig.perfCounters.gpuMemoryLimit = m_perfCounterMemLimit;
            sampleConfig.perfCounters.spmTraceSampleInterval = m_perfCounterFrequency;
            sampleConfig.perfCounters.numCounters = m_perfCounterIds.NumElements();
            sampleConfig.perfCounters.pIds = m_perfCounterIds.Data();
        }

        result = pState->pGpaSession->BeginSample(pBeginCmdBuf, sampleConfig, &pState->gpaSampleId);
    }

    if (result == Pal::Result::Success)
    {
        GpuUtil::SampleTraceApiInfo sampleTraceApiInfo = {};

        switch (m_triggerMode)
        {
        case TriggerMode::Present:
            sampleTraceApiInfo.profilingMode = GpuUtil::TraceProfilingMode::Present;
            break;
        case TriggerMode::Tag:
            sampleTraceApiInfo.profilingMode = GpuUtil::TraceProfilingMode::Tags;
            sampleTraceApiInfo.profilingModeData.tagData.start = m_traceFrameBeginTag;
            sampleTraceApiInfo.profilingModeData.tagData.end = m_traceFrameEndTag;
            break;
        case TriggerMode::Index:
            sampleTraceApiInfo.profilingMode = GpuUtil::TraceProfilingMode::FrameNumber;
            sampleTraceApiInfo.profilingModeData.frameNumberData.start = m_traceFrameBeginIndex;
            sampleTraceApiInfo.profilingModeData.frameNumberData.end = m_traceFrameEndIndex;
            break;
        default:
            VK_NOT_IMPLEMENTED;
        }

        if (m_enableInstTracing)
        {
            sampleTraceApiInfo.instructionTraceMode = m_targetApiPsoHash == 0 ?
                GpuUtil::InstructionTraceMode::FullFrame :
                GpuUtil::InstructionTraceMode::ApiPso;

            sampleTraceApiInfo.instructionTraceModeData.apiPsoHash = m_targetApiPsoHash;
        }
        else
        {
            sampleTraceApiInfo.instructionTraceMode = GpuUtil::InstructionTraceMode::Disabled;
        }

        if (settings.devModeSqttInstructionTraceEnable)
        {
            sampleTraceApiInfo.instructionTraceMode = settings.devModeSqttTargetApiPsoHash == 0 ?
                GpuUtil::InstructionTraceMode::FullFrame :
                GpuUtil::InstructionTraceMode::ApiPso;

            sampleTraceApiInfo.instructionTraceModeData.apiPsoHash = settings.devModeSqttTargetApiPsoHash;
        }

        pState->pGpaSession->SetSampleTraceApiInfo(sampleTraceApiInfo, pState->gpaSampleId);
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
            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
            perSubQueueInfo.cmdBufferCount             = 1;
            perSubQueueInfo.ppCmdBuffers               = &pTracePrepareQueue->pFamily->pTraceBeginCmdBuf;
            perSubQueueInfo.pCmdBufInfoList            = nullptr;

            Pal::SubmitInfo submitInfo      = {};
            submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
            submitInfo.perSubQueueInfoCount = 1;
            submitInfo.fenceCount           = 0;

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

        // If the app is tracing using tags, we need to immediately block on trace end
        // (rather than periodically checking during future present calls).
        m_blockingTraceEnd |= (delimiterType == FrameDelimiterType::CmdBufferTag);

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
    VK_ASSERT(IsTracingEnabled());

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
                Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};

                perSubQueueInfo.cmdBufferCount  = 1;
                perSubQueueInfo.ppCmdBuffers    = m_enableSampleUpdates ? &pTraceQueue->pFamily->pTraceBeginSqttCmdBuf
                                                                        : &pTraceQueue->pFamily->pTraceBeginCmdBuf;
                perSubQueueInfo.pCmdBufInfoList = nullptr;

                Pal::SubmitInfo submitInfo = {};

                submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
                submitInfo.perSubQueueInfoCount = 1;
                submitInfo.ppFences             = &pState->pBeginFence;
                submitInfo.fenceCount           = 1;

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
                            Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
                            perSubQueueInfo.cmdBufferCount  = 1;
                            perSubQueueInfo.ppCmdBuffers    = &pFamilyState->pTraceFlushCmdBuf;
                            perSubQueueInfo.pCmdBufInfoList = nullptr;

                            // Submit the flush command buffer
                            Pal::SubmitInfo submitInfo = {};

                            submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
                            submitInfo.perSubQueueInfoCount = 1;
                            submitInfo.fenceCount           = 0;

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
    // also take into account if a specific number of frames has been requested
    // through Index mode
    uint32_t requestedFrames =
        m_trace.pDevice->GetRuntimeSettings().devModeSqttFrameCount;
    if (m_triggerMode == TriggerMode::Index)
    {
        requestedFrames = (m_traceFrameBeginIndex < m_traceFrameEndIndex) ?
            m_traceFrameEndIndex - m_traceFrameBeginIndex : 0u;
    }
    if (m_trace.sqttFrameCount < requestedFrames)
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
        Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};
        perSubQueueInfo.cmdBufferCount  = 1;
        perSubQueueInfo.ppCmdBuffers    = &pEndSqttCmdBuf;
        perSubQueueInfo.pCmdBufInfoList = nullptr;

        Pal::SubmitInfo submitInfo = {};
        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.ppFences             = &pState->pEndSqttFence;
        submitInfo.fenceCount           = 1;

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

        Pal::PerSubQueueSubmitInfo perSubQueueInfo = {};

        perSubQueueInfo.cmdBufferCount  = 1;
        perSubQueueInfo.ppCmdBuffers    = &pEndCmdBuf;
        perSubQueueInfo.pCmdBufInfoList = nullptr;

        Pal::SubmitInfo submitInfo = {};

        submitInfo.pPerSubQueueInfo     = &perSubQueueInfo;
        submitInfo.perSubQueueInfoCount = 1;
        submitInfo.ppFences             = &pState->pEndFence;
        submitInfo.fenceCount           = 1;

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

    // Deactivate static VMID if supported (and currently active)
    if (m_useStaticVmid && m_staticVmidActive)
    {
        Pal::Result palResult = pState->pDevice->PalDevice(DefaultDeviceIndex)->SetStaticVmidMode(false);
        VK_ASSERT(palResult == Pal::Result::Success);
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
// This function initializes the resources necessary for capturing queue timing data from a given queue.
//
// If "auxQueue" is true, then the queue provided does not belong to the tracing logical device, but belongs to the
// same physical device (and thus, the same PAL device)
Pal::Result DevModeMgr::InitTraceQueueResources(
    TraceState*  pState,
    bool*        pHasDebugVmid,
    const Queue* pQueue,
    bool         auxQueue)
{
    Pal::Result result = Pal::Result::Success;

    // Has this queue's family been previously seen?
    uint32_t familyIdx = pQueue->GetFamilyIndex();
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
        pFamilyState->supportsTracing = SqttMgr::IsTracingSupported(
            pState->pDevice->VkPhysicalDevice(DefaultDeviceIndex), familyIdx);
        pFamilyState->queueType = pState->pDevice->GetQueueFamilyPalQueueType(familyIdx);
        pFamilyState->engineType = pState->pDevice->GetQueueFamilyPalEngineType(familyIdx);

        // Initialize resources for this queue family
        result = InitTraceQueueFamilyResources(pState, pFamilyState);
    }

    if (result == Pal::Result::Success)
    {
        uint32_t* pQueueStateCount = auxQueue ? &pState->auxQueueCount : &pState->queueCount;

        if ((*pQueueStateCount) < MaxTraceQueues)
        {
            (*pQueueStateCount)++;
        }
        else
        {
            result = Pal::Result::ErrorUnavailable;
        }
    }

    // Register this queue for timing operations
    if (result == Pal::Result::Success)
    {
        TraceQueueState* pQueueState = auxQueue ?
            &pState->auxQueueStates[pState->auxQueueCount - 1] : &pState->queueState[pState->queueCount - 1];

        pQueueState->pQueue = pQueue;
        pQueueState->pFamily = pFamilyState;
        pQueueState->timingSupported = false;
        pQueueState->queueId = reinterpret_cast<Pal::uint64>(ApiQueue::FromObject(pQueue));

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

    return result;
}

// =====================================================================================================================
// This function finds out all the queues in the device that we have to synchronize for RGP-traced frames and
// initializes resources for them.
Pal::Result DevModeMgr::InitTraceQueueResourcesForDevice(
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

                pState->pDevice->GetQueue(familyIdx, queueIdx, &queueHandle);

                if (queueHandle != VK_NULL_HANDLE)
                {
                    Queue* pQueue = ApiQueue::ObjectFromHandle(queueHandle);

                    result = InitTraceQueueResources(pState, pHasDebugVmid, pQueue, false);
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
// Initializes device-persistent RGP resources
Pal::Result DevModeMgr::InitRGPTracing(
    TraceState* pState,
    Device*     pDevice)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    Pal::Result result = Pal::Result::Success;

    if ((IsTracingEnabled() == false) ||  // Tracing is globally disabled
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
    if (pDevice->GetRuntimeSettings().devModeSqttForceDisable)
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

        // Initialize large embedded data chunk allocation size
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocHeap    = settings.cmdAllocatorEmbeddedHeap;
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].allocSize    = settings.cmdAllocatorLargeEmbeddedAllocSize;
        createInfo.allocInfo[Pal::LargeEmbeddedDataAlloc].suballocSize =
            settings.cmdAllocatorLargeEmbeddedSubAllocSize;

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
                GpuUtil::ApiType::Vulkan,
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
        result = InitTraceQueueResourcesForDevice(pState, &hasDebugVmid);
    }

    if (result == Pal::Result::Success)
    {
        m_useStaticVmid = (pDevice->GetPalProperties().gfxipProperties.flags.supportStaticVmid != 0);

        // If we've failed to acquire the debug VMID (and it is needed), fail to trace
        if ((hasDebugVmid == false) && (m_useStaticVmid == false))
        {
            result = Pal::Result::ErrorInitializationFailed;
        }
    }

    if (result != Pal::Result::Success)
    {
        // If we've failed to initialize tracing, permanently disable traces
        if (m_pRGPServer != nullptr)
        {
            m_pRGPServer->DisableTraces();
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
        pDriverControlServer->FinishDeviceInit();
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
                           (pQueue->VkDevice()->VkPhysicalDevice(DefaultDeviceIndex) ==
                               m_trace.pDevice->VkPhysicalDevice(DefaultDeviceIndex));

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
        result = m_trace.pGpaSession->TimedSignalQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->SignalQueueSemaphore(pQueueSemaphore, value);
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
        result = m_trace.pGpaSession->TimedWaitQueueSemaphore(pPalQueue, pQueueSemaphore, timedSemaphoreInfo, value);

        VK_ASSERT(result == Pal::Result::Success);
    }

    if (result != Pal::Result::Success)
    {
        result = pPalQueue->WaitQueueSemaphore(pQueueSemaphore, value);
    }

    return result;
}

// =====================================================================================================================
bool DevModeMgr::IsTracingEnabled() const
{
    VK_ASSERT(m_finalized);

    if (m_finalized)
    {
        return (m_pRGPServer != nullptr) && m_pRGPServer->TracesEnabled();
    }
    else
    {
        return false;
    }
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
    VK_ASSERT(cmdBufferCount == submitInfo.pPerSubQueueInfo[0].cmdBufferCount);

    bool timingSupported = QueueSupportsTiming(deviceIdx, pQueue) && (submitInfo.pPerSubQueueInfo[0].cmdBufferCount > 0);

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

            VK_ASSERT(pCmdBuf->PalCmdBuffer(DefaultDeviceIndex) == submitInfo.pPerSubQueueInfo[0].ppCmdBuffers[cbIdx]);
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
        GpuUtil::RegisterPipelineInfo pipelineInfo = { 0 };
        pipelineInfo.apiPsoHash = pPipeline->GetApiHash();

        m_trace.pGpaSession->RegisterPipeline(pPipeline->PalPipeline(DefaultDeviceIndex), pipelineInfo);
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

#if VKI_RAY_TRACING
// =====================================================================================================================
// Registers the shader libraries under this pipeline so the contents of each library can be written into the RGP
// trace file.
void DevModeMgr::ShaderLibrariesCreated(
    Device*             pDevice,
    RayTracingPipeline* pPipeline)
{
    if ((m_trace.pDevice == pDevice) &&
        m_trace.pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (m_trace.pGpaSession != nullptr))
    {
        for (uint32_t i = 0; i < pPipeline->GetShaderLibraryCount(); ++i)
        {
            GpuUtil::RegisterLibraryInfo pipelineInfo = { pPipeline->GetApiHash() };
            m_trace.pGpaSession->RegisterLibrary(pPipeline->PalShaderLibrary(i), pipelineInfo);
        }
    }
}

// =====================================================================================================================
// Unregisters the shader libraries under this pipeline, recording an unload event in the RGP trace.
void DevModeMgr::ShaderLibrariesDestroyed(
    Device*             pDevice,
    RayTracingPipeline* pPipeline)
{
    if ((m_trace.pDevice == pDevice) &&
        m_trace.pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (m_trace.pGpaSession != nullptr))
    {
        for (uint32_t i = 0; i < pPipeline->GetShaderLibraryCount(); ++i)
        {
            m_trace.pGpaSession->UnregisterLibrary(pPipeline->PalShaderLibrary(i));
        }
    }
}
#endif

// =====================================================================================================================
// Retrieves the target API PSO hash from the RGP Server
uint64_t DevModeMgr::GetInstructionTraceTargetHash()
{
    uint64_t targetHash = InvalidTargetPipelineHash;

    if (IsTracingEnabled())
    {
        const auto& settings = m_trace.pDevice->GetRuntimeSettings();
        const auto  traceParameters = m_pRGPServer->QueryTraceParameters();

        targetHash = settings.devModeSqttInstructionTraceEnable ?
            settings.devModeSqttTargetApiPsoHash :
            traceParameters.pipelineHash;
    }

    return targetHash;
}

// =====================================================================================================================
// Starts instruction trace
void DevModeMgr::StartInstructionTrace(
    CmdBuffer* pCmdBuffer)
{
    if (IsTracingEnabled())
    {
        m_trace.pGpaSession->UpdateSampleTraceParams(
            pCmdBuffer->PalCmdBuffer(DefaultDeviceIndex),
            0,
            GpuUtil::UpdateSampleTraceMode::StartInstructionTrace);
    }
}

// =====================================================================================================================
// Stops instruction trace
void DevModeMgr::StopInstructionTrace(
    CmdBuffer* pCmdBuffer)
{
    if (IsTracingEnabled())
    {
        m_trace.pGpaSession->UpdateSampleTraceParams(
            pCmdBuffer->PalCmdBuffer(DefaultDeviceIndex),
            0,
            GpuUtil::UpdateSampleTraceMode::StopInstructionTrace);
    }
}

// =====================================================================================================================
// Registers a pipeline binary cache object with the pipeline URI service and initializes the pipeline URI service
// the first time a pipeline binary cache object is registered
Util::Result DevModeMgr::RegisterPipelineCache(
    PipelineBinaryCache* pPipelineCache,
    uint32_t             postSizeLimit)
{
    Util::Result result = Util::Result::Success;

    if (m_pPipelineUriService == nullptr)
    {
        void* pStorage = m_pInstance->AllocMem(sizeof(DevDriver::PipelineUriService), VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);

        if (pStorage != nullptr)
        {
            m_pPipelineUriService = VK_PLACEMENT_NEW(pStorage) DevDriver::PipelineUriService();
        }
        else
        {
            result = Util::Result::ErrorOutOfMemory;
        }

        if (result == Util::Result::Success)
        {
            DevDriver::PipelineUriService::DriverInfo driverInfo;
            driverInfo.pUserData = static_cast<void*>(this);
            driverInfo.pfnGetPipelineHashes = &GetPipelineHashes;
            driverInfo.pfnGetPipelineCodeObjects = &GetPipelineCodeObjects;
            driverInfo.pfnInjectPipelineCodeObjects = &InjectPipelineCodeObjects;
            driverInfo.postSizeLimit = postSizeLimit * 1024;

            DevDriver::Result devDriverResult = m_pPipelineUriService->Init(driverInfo);

            if (devDriverResult == DevDriver::Result::Success)
            {
                devDriverResult = m_pDevDriverServer->GetMessageChannel()->RegisterService(m_pPipelineUriService);
            }

            if (devDriverResult != DevDriver::Result::Success)
            {
                result = Util::Result::ErrorUnavailable;
            }
        }
    }

    if (result == Util::Result::Success)
    {
        Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> readWriteLock(&m_pipelineReinjectionLock);

        result = m_pipelineCaches.PushBack(pPipelineCache);
    }

    return result;
}

// =====================================================================================================================
// Deregisters a pipeline binary cache with the pipeline URI service
void DevModeMgr::DeregisterPipelineCache(
    PipelineBinaryCache* pPipelineCache)
{
    Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> readWriteLock(&m_pipelineReinjectionLock);

    auto it = m_pipelineCaches.Begin();

    while (it.Get() != nullptr)
    {
        PipelineBinaryCache* element = *it.Get();

        if (pPipelineCache == element)
        {
            m_pipelineCaches.Erase(&it);

            // Each element should only be in the list once; break out of loop once found
            break;
        }
        else
        {
            it.Next();
        }
    }
}

}; // namespace vk

#endif
