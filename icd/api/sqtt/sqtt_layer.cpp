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
 * @file  sqtt_layer.cpp
 * @brief Implementation of the SQTT layer.  This layer is an internal driver layer (not true loader-aware layer)
 *        that intercepts certain API calls to insert metadata tokens into the command stream while SQ thread tracing
 *        is active for the purposes of developer mode RGP profiling.
 ***********************************************************************************************************************
 */

#include "devmode/devmode_mgr.h"
#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"
#include "include/vk_instance.h"
#include "include/vk_extensions.h"
#if VKI_RAY_TRACING
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/ray_tracing_device.h"
#endif
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

#include "palEventDefs.h"
#include "palHashMapImpl.h"
#include "palListImpl.h"

namespace vk
{

static_assert(RgpSqttMarkerCbStartWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerCbStart),
    "Marker size mismatch");
static_assert(RgpSqttMarkerCbEndWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerCbEnd),
    "Marker size mismatch");
static_assert(RgpSqttMarkerEventWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerEvent),
    "Marker size mismatch");
static_assert(RgpSqttMarkerEventWithDimsWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerEventWithDims),
    "Marker size mismatch");
static_assert(RgpSqttMarkerBarrierStartWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerBarrierStart),
    "Marker size mismatch");
static_assert(RgpSqttMarkerBarrierEndWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerBarrierEnd),
    "Marker size mismatch");
static_assert(RgpSqttMarkerLayoutTransitionWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerLayoutTransition),
    "Marker size mismatch");
static_assert(RgpSqttMarkerUserEventWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerUserEvent),
    "Marker size mismatch");
static_assert(RgpSqttMarkerGeneralApiWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerGeneralApi),
    "Marker size mismatch");
static_assert(RgpSqttMarkerPresentWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerPresent),
    "Marker size mismatch");
static_assert(RgpSqttMarkerPipelineBindWordCount * sizeof(uint32_t) == sizeof(RgpSqttMarkerPipelineBind),
    "Marker size mismatch");

constexpr uint8 MarkerSourceApplication = 0;

// =====================================================================================================================
// Construct per-queue SQTT layer info
SqttQueueState::SqttQueueState(
    Queue* pQueue)
    :
    m_pQueue(pQueue),
    m_pDevice(pQueue->VkDevice()),
    m_cmdBufferMap(32, pQueue->VkDevice()->VkInstance()->GetPrivateAllocator()),
    m_pNextLayer(pQueue->VkDevice()->GetSqttMgr()->GetNextLayer())
{
    m_enabledMarkers = m_pDevice->GetRuntimeSettings().devModeSqttMarkerEnable;

    if (SqttMgr::IsTracingSupported(m_pDevice->VkPhysicalDevice(DefaultDeviceIndex), pQueue->GetFamilyIndex()) == false)
    {
        m_enabledMarkers = 0;
    }
}

// =====================================================================================================================
// Initialize per-queue SQTT layer info
VkResult SqttQueueState::Init()
{
    Util::Result palResult = m_cmdBufferMap.Init();

    VkResult result = PalToVkResult(palResult);

    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = m_pQueue->GetFamilyIndex();

    result = CmdPool::Create(
        m_pDevice,
        &poolCreateInfo,
        &vk::allocator::g_DefaultAllocCallback,
        &m_cmdPool);

    return result;
}

// =====================================================================================================================
void SqttQueueState::DebugLabelBegin(const VkDebugUtilsLabelEXT* pMarkerInfo)
{
    SubmitUserEventMarker(RgpSqttMarkerUserEventPush, pMarkerInfo->pLabelName);
}

// =====================================================================================================================
void SqttQueueState::DebugLabelEnd()
{
    SubmitUserEventMarker(RgpSqttMarkerUserEventPop, nullptr);
}

// =====================================================================================================================
void SqttQueueState::DebugLabelInsert(const VkDebugUtilsLabelEXT* pMarkerInfo)
{
    SubmitUserEventMarker(RgpSqttMarkerUserEventTrigger, pMarkerInfo->pLabelName);
}

// =====================================================================================================================
// Submits a user event string queue marker
void SqttQueueState::SubmitUserEventMarker(RgpSqttMarkerUserEventType eventType, const char* pString)
{
    VkResult         result    = VK_SUCCESS;
    VkCommandBuffer  cmdBuffer = VK_NULL_HANDLE;
    VkCommandBuffer* pEntry    = nullptr;

    CmdBufferMapKey key;
    key.u64All = 0;
    key.eventType = eventType;

    if (pString != nullptr)
    {
        key.stringHash = Util::HashString(pString, strlen(pString));
    }

    {
        Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> readLock(&m_lock);

        pEntry = m_cmdBufferMap.FindKey(key.u64All);

        if (pEntry != nullptr)
        {
            // Command buffer found in cache
            cmdBuffer = *pEntry;
        }
    }

    if (pEntry == nullptr)
    {
        Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> readWriteLock(&m_lock);

        // Build command buffer and cache it
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = m_cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        result = CmdBuffer::Create(
            m_pDevice,
            &allocInfo,
            &cmdBuffer);

        CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);

        if (result == VK_SUCCESS)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

            result = pCmdBuffer->Begin(&beginInfo);
        }

        if (result == VK_SUCCESS)
        {
            pCmdBuffer->GetSqttState()->WriteUserEventMarker(eventType, pString);

            result = pCmdBuffer->End();
        }

        if (result == VK_SUCCESS)
        {
            m_cmdBufferMap.Insert(key.u64All, cmdBuffer);
        }
    }

    if (result == VK_SUCCESS)
    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        result = m_pQueue->Submit(1, &submitInfo, VK_NULL_HANDLE);
    }

    VK_ASSERT(result == VK_SUCCESS);
}

// =====================================================================================================================
// Writes a present API marker
void SqttQueueState::WritePresentMarker(
    Pal::ICmdBuffer* pCmdBuffer,
    bool*            pAddedMarker
    ) const
{
    if ((m_enabledMarkers & DevModeSqttMarkerEnablePresent) != 0)
    {
        RgpSqttMarkerPresent marker = {};

        marker.identifier = RgpSqttMarkerIdentifierPresent;

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        pCmdBuffer->CmdInsertRgpTraceMarker(
            subQueueFlags,
            static_cast<uint32_t>(sizeof(marker) / sizeof(uint32_t)),
            &marker);

        if (pAddedMarker != nullptr)
        {
            *pAddedMarker = true;
        }
    }
}

// =====================================================================================================================
// Destroy per-queue SQTT layer info
SqttQueueState::~SqttQueueState()
{
    VkResult result = VK_SUCCESS;

    for (CmdBufferMap::Iterator iter = m_cmdBufferMap.Begin(); iter.Get() != nullptr; iter.Next())
    {
        ApiCmdBuffer::ObjectFromHandle(iter.Get()->value)->Destroy();
    }

    result = CmdPool::ObjectFromHandle(m_cmdPool)->Destroy(
        m_pDevice,
        &vk::allocator::g_DefaultAllocCallback);

    VK_ASSERT(result == VK_SUCCESS);
}

// =====================================================================================================================
// Initialize per-cmdbuf SQTT layer info
SqttCmdBufferState::SqttCmdBufferState(
    CmdBuffer* pCmdBuf)
    :
    m_pCmdBuf(pCmdBuf),
    m_pSqttMgr(pCmdBuf->VkDevice()->GetSqttMgr()),
    m_pDevModeMgr(pCmdBuf->VkDevice()->VkInstance()->GetDevModeMgr()),
    m_settings(pCmdBuf->VkDevice()->GetRuntimeSettings()),
    m_pNextLayer(m_pSqttMgr->GetNextLayer()),
    m_currentEntryPoint(RgpSqttMarkerGeneralApiType::Invalid),
    m_currentEventId(0),
    m_currentEventType(RgpSqttMarkerEventType::InternalUnknown),
#if ICD_GPUOPEN_DEVMODE_BUILD
    m_instructionTrace({ false, DevModeMgr::InvalidTargetPipelineHash, VK_PIPELINE_BIND_POINT_MAX_ENUM }),
#endif
    m_debugTags(pCmdBuf->VkInstance()->Allocator())
{
    m_cbId.u32All       = 0;
    m_deviceId          = reinterpret_cast<uint64_t>(ApiDevice::FromObject(m_pCmdBuf->VkDevice()));
    m_queueFamilyIndex  = m_pCmdBuf->GetQueueFamilyIndex();

    uint32_t queueCount = Queue::MaxQueueFamilies;
    VkQueueFamilyProperties queueProps[Queue::MaxQueueFamilies] = {};

    VkResult result = m_pCmdBuf->VkDevice()->VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyProperties(&queueCount, queueProps);

    VK_ASSERT(result == VK_SUCCESS);
    VK_ASSERT(m_queueFamilyIndex < queueCount);

    m_queueFamilyFlags = queueProps[m_queueFamilyIndex].queueFlags;

    ResetBarrierState();

    m_enabledMarkers = m_pCmdBuf->VkDevice()->GetRuntimeSettings().devModeSqttMarkerEnable;

    if (SqttMgr::IsTracingSupported(m_pCmdBuf->VkDevice()->VkPhysicalDevice(DefaultDeviceIndex), m_queueFamilyIndex) == false)
    {
        m_enabledMarkers = 0;
    }

    m_pUserEvent = reinterpret_cast<RgpSqttMarkerUserEventWithString*>(m_pCmdBuf->VkInstance()->AllocMem(
        sizeof(*m_pUserEvent), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));
}

// =====================================================================================================================
// Inserts CbStart marker when a new command buffer is begun.
void SqttCmdBufferState::Begin(
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    m_currentEventId = 0;

#if ICD_GPUOPEN_DEVMODE_BUILD
    if (m_pDevModeMgr != nullptr)
    {
        m_instructionTrace.targetHash = m_pDevModeMgr->GetInstructionTraceTargetHash();
    }
#endif

    m_cbId = m_pSqttMgr->GetNextCmdBufID(m_pCmdBuf->GetQueueFamilyIndex(), pBeginInfo);

    // Clear the list of debug tags whenever a new command buffer is started.
    auto it = m_debugTags.Begin();
    while (it.Get() != nullptr)
    {
        m_debugTags.Erase(&it);
    }

    WriteCbStartMarker();
}

// =====================================================================================================================
// Inserts a CbEnd marker when command buffer building has finished.
void SqttCmdBufferState::End()
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    // If instruction tracing was enabled for this Command List,
    // insert a barrier used to wait for all trace data to finish writing.
    if (m_instructionTrace.started && m_settings.rgpInstTraceBarrierEnabled)
    {
        // Select the pipe point based on the bound pipeline's type.
        Pal::HwPipePoint pipePoint = Pal::HwPipeTop;
        if (m_instructionTrace.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            pipePoint = Pal::HwPipePostPs;
        }
        else if (m_instructionTrace.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
        {
            pipePoint = Pal::HwPipePostCs;
        }
        else
        {
            // Invalid pipeline type.
            PAL_ASSERT_ALWAYS();
        }

        Pal::BarrierInfo barrierInfo   = {};
        barrierInfo.waitPoint          = Pal::HwPipeTop;
        barrierInfo.pipePointWaitCount = 1;
        barrierInfo.pPipePoints        = &pipePoint;
        barrierInfo.reason             = RgpBarrierInternalInstructionTraceStall;

        m_pCmdBuf->PalCmdBuffer(DefaultDeviceIndex)->CmdBarrier(barrierInfo);
    }
#endif

    WriteCbEndMarker();

#if ICD_GPUOPEN_DEVMODE_BUILD
    if ((m_pDevModeMgr != nullptr) &&
        (m_instructionTrace.started))
    {
        m_pDevModeMgr->StopInstructionTrace(m_pCmdBuf);
        m_instructionTrace.started = false;
    }
#endif
}

// =====================================================================================================================
// Sets up an Event marker's basic data.
RgpSqttMarkerEvent SqttCmdBufferState::BuildEventMarker(
    RgpSqttMarkerEventType apiType)
{
    RgpSqttMarkerEvent marker = {};

    marker.identifier  = RgpSqttMarkerIdentifierEvent;
    marker.apiType     = static_cast<uint32_t>(apiType);
    marker.cmdID       = m_currentEventId++;
    marker.cbID        = m_cbId.u32All;

    return marker;
}

// =====================================================================================================================
void SqttCmdBufferState::WriteMarker(
    const void*                 pData,
    size_t                      dataSize,
    Pal::RgpMarkerSubQueueFlags subQueueFlags
    ) const
{
    VK_ASSERT(m_enabledMarkers != 0);
    VK_ASSERT((dataSize % sizeof(uint32_t)) == 0);
    VK_ASSERT((dataSize / sizeof(uint32_t)) > 0);

    m_pCmdBuf->PalCmdBuffer(DefaultDeviceIndex)->CmdInsertRgpTraceMarker(
        subQueueFlags,
        static_cast<uint32_t>(dataSize / sizeof(uint32_t)),
        pData);
}

// =====================================================================================================================
// This function begins a sequence where one or more draws/dispatches may be generated for a particular reason
// (described by the API type).  Each of these draws/dispatches will be associated with their own RGP event marker
// associated with an event ID.
void SqttCmdBufferState::BeginEventMarkers(
    RgpSqttMarkerEventType apiType)
{
    VK_ASSERT(m_currentEventType == RgpSqttMarkerEventType::InternalUnknown);

    m_currentEventType = apiType;
}

// =====================================================================================================================
// This function ends a Begin/End pre-draw/dispatch event marker sequence.  See BeginEventMarkers() for details.
void SqttCmdBufferState::EndEventMarkers()
{
    m_currentEventType = RgpSqttMarkerEventType::InternalUnknown;
}

// =====================================================================================================================
// Inserts an RGP pre-draw/dispatch marker.
void SqttCmdBufferState::WriteEventMarker(
    RgpSqttMarkerEventType      apiType,
    uint32_t                    vertexOffsetUserData,
    uint32_t                    instanceOffsetUserData,
    uint32_t                    drawIndexUserData,
    Pal::RgpMarkerSubQueueFlags subQueueFlags
    )
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableEvent)
    {
        VK_ASSERT(apiType != RgpSqttMarkerEventType::Invalid);

        RgpSqttMarkerEvent marker = BuildEventMarker(apiType);

        if ((vertexOffsetUserData == UINT_MAX) || (instanceOffsetUserData == UINT_MAX))
        {
            vertexOffsetUserData   = 0;
            instanceOffsetUserData = 0;
        }

        if (drawIndexUserData == UINT_MAX)
        {
            drawIndexUserData = vertexOffsetUserData;
        }

        marker.vertexOffsetRegIdx   = vertexOffsetUserData;
        marker.instanceOffsetRegIdx = instanceOffsetUserData;
        marker.drawIndexRegIdx      = drawIndexUserData;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Inserts an RGP pre-dispatch marker
void SqttCmdBufferState::WriteEventWithDimsMarker(
    RgpSqttMarkerEventType      apiType,
    uint32_t                    x,
    uint32_t                    y,
    uint32_t                    z,
    Pal::RgpMarkerSubQueueFlags subQueueFlags
    )
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableEvent)
    {
        VK_ASSERT(apiType != RgpSqttMarkerEventType::Invalid);

        RgpSqttMarkerEventWithDims eventWithDims = {};

        eventWithDims.event               = BuildEventMarker(apiType);
        eventWithDims.event.hasThreadDims = 1;
        eventWithDims.threadX             = x;
        eventWithDims.threadY             = y;
        eventWithDims.threadZ             = z;

        WriteMarker(&eventWithDims, sizeof(eventWithDims), subQueueFlags);
    }
}

// =====================================================================================================================
// Inserts a user event string marker
void SqttCmdBufferState::WriteUserEventMarker(
    RgpSqttMarkerUserEventType eventType,
    const char*                pString
    ) const
{
    if ((m_enabledMarkers & DevModeSqttMarkerEnableUserEvent) &&
        (m_pUserEvent != nullptr))
    {
        memset(m_pUserEvent, 0, sizeof(*m_pUserEvent));

        m_pUserEvent->header.identifier = RgpSqttMarkerIdentifierUserEvent;
        m_pUserEvent->header.dataType   = eventType;

        size_t markerSize = sizeof(m_pUserEvent->header);

        if ((eventType != RgpSqttMarkerUserEventPop))
        {
            size_t strLength = 0;

            // Copy and encode the string if one exists
            if (pString != nullptr)
            {
                strLength = Util::Min(strlen(pString), RgpSqttMaxUserEventStringLengthInDwords * sizeof(uint32_t));

                for (uint32_t charIdx = 0; charIdx < strLength; ++charIdx)
                {
                    uint32_t c = static_cast<uint32_t>(pString[charIdx]);

                    m_pUserEvent->stringData[charIdx / 4] |= (c << (8 * (charIdx % 4)));
                }

                m_pUserEvent->stringLength = static_cast<uint32_t>(strLength);
            }

            // Every data type other than Pop includes a string length
            markerSize += sizeof(uint32_t);

            // Include string length (padded up to the nearest dword)
            markerSize += sizeof(uint32_t) * ((strLength + sizeof(uint32_t) - 1) / sizeof(uint32_t));
        }

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(m_pUserEvent, markerSize, subQueueFlags);
    }
}

// =====================================================================================================================
void SqttCmdBufferState::ResetBarrierState()
{
    m_currentBarrier.caches               = 0;
    m_currentBarrier.pipelineStalls       = 0;
    m_currentBarrier.numLayoutTransitions = 0;
    m_currentBarrier.inside               = false;
}

// =====================================================================================================================
// Writes SQTT marker data based on PAL barrier callbacks.
void SqttCmdBufferState::PalBarrierCallback(
    Pal::Developer::CallbackType       type,
    const Pal::Developer::BarrierData& barrier)
{
    // Include cache/stall data from this callback to the current barrier state.  The union
    // of all of this will be written during the BarrierEnd marker.  The reason this is necessary
    // is because sometimes PAL callbacks for layout transitions (ImageBarrier) also includes
    // cache flush data.
    m_currentBarrier.pipelineStalls |= barrier.operations.pipelineStalls.u16All;
    m_currentBarrier.caches         |= barrier.operations.caches.u16All;

    // Write a marker corresponding with the barrier state
    switch (type)
    {
    case Pal::Developer::CallbackType::BarrierBegin:
        m_currentBarrier.inside = true;

        WriteBarrierStartMarker(barrier);
        break;

    case Pal::Developer::CallbackType::BarrierEnd:
        WriteBarrierEndMarker(barrier);
        ResetBarrierState();
        break;

    case Pal::Developer::CallbackType::ImageBarrier:
        WriteLayoutTransitionMarker(barrier);
        m_currentBarrier.numLayoutTransitions++;
        break;

    default:
        VK_NEVER_CALLED();
        break;
    }
}

// =====================================================================================================================
// Writes SQTT marker data based on PAL draw/dispatch callbacks.
void SqttCmdBufferState::PalDrawDispatchCallback(
    const Pal::Developer::DrawDispatchData& drawDispatch)
{
    constexpr uint32_t FirstDispatchType = static_cast<uint32_t>(Pal::Developer::DrawDispatchType::FirstDispatch);

    // Draw call
    if (static_cast<uint32_t>(drawDispatch.cmdType) < FirstDispatchType)
    {
        WriteEventMarker(
            m_currentEventType,
            drawDispatch.draw.userDataRegs.firstVertex,
            drawDispatch.draw.userDataRegs.instanceOffset,
            drawDispatch.draw.userDataRegs.drawIndex,
            drawDispatch.subQueueFlags);
    }
    // Dispatch call
    else
    {
        const auto& settings = m_pCmdBuf->VkDevice()->GetRuntimeSettings();

        // These types of dispatches have the compute dimensions included
        if ((settings.devModeSqttMarkerEnable & DevModeSqttMarkerEnableEventWithDims) &&
            ((drawDispatch.cmdType == Pal::Developer::DrawDispatchType::CmdDispatch) ||
             (drawDispatch.cmdType == Pal::Developer::DrawDispatchType::CmdDispatchOffset)))
        {
            WriteEventWithDimsMarker(
                m_currentEventType,
                drawDispatch.dispatch.groupDims.x,
                drawDispatch.dispatch.groupDims.y,
                drawDispatch.dispatch.groupDims.z,
                drawDispatch.subQueueFlags);
        }
        else
        {
            WriteEventMarker(
                m_currentEventType,
                UINT_MAX,
                UINT_MAX,
                UINT_MAX,
                drawDispatch.subQueueFlags);
        }
    }
}

// =====================================================================================================================
// Writes SQTT marker data based on PAL bind pipeline callbacks.
void SqttCmdBufferState::PalBindPipelineCallback(
    const Pal::Developer::BindPipelineData& bindPipeline)
{
    WritePipelineBindMarker(bindPipeline);
}

// =====================================================================================================================
void SqttCmdBufferState::WriteBarrierStartMarker(
    const Pal::Developer::BarrierData& data
    ) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableBarrier)
    {
        RgpSqttMarkerBarrierStart marker = {};

        marker.identifier = RgpSqttMarkerIdentifierBarrierStart;
        marker.cbId       = m_cbId.u32All;
        marker.dword02    = data.reason;

        // The barrier reason should never be 0.  It indicates somebody is not giving a correct reason before
        // calling Pal::CmdBarrier().
        if (marker.dword02 == 0)
        {
            VK_NEVER_CALLED();

            marker.dword02 = RgpBarrierUnknownReason;
        }

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
void SqttCmdBufferState::WriteLayoutTransitionMarker(
    const Pal::Developer::BarrierData& data
    ) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableBarrier)
    {
        RgpSqttMarkerLayoutTransition marker = {};

        marker.identifier                   = RgpSqttMarkerIdentifierLayoutTransition;
        marker.depthStencilExpand           = data.operations.layoutTransitions.depthStencilExpand;
        marker.htileHiZRangeExpand          = data.operations.layoutTransitions.htileHiZRangeExpand;
        marker.depthStencilResummarize      = data.operations.layoutTransitions.depthStencilResummarize;
        marker.dccDecompress                = data.operations.layoutTransitions.dccDecompress;
        marker.fmaskDecompress              = data.operations.layoutTransitions.fmaskDecompress;
        marker.fastClearEliminate           = data.operations.layoutTransitions.fastClearEliminate;
        marker.fmaskColorExpand             = data.operations.layoutTransitions.fmaskColorExpand;
        marker.initMaskRam                  = data.operations.layoutTransitions.initMaskRam;

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
void SqttCmdBufferState::WriteBarrierEndMarker(
    const Pal::Developer::BarrierData& data
    ) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableBarrier)
    {
        // Copy the operations part and include the same data from previous markers
        // within the same barrier sequence to create a full picture of all cache
        // syncs and pipeline stalls.
        auto operations = data.operations;

        operations.pipelineStalls.u16All |= m_currentBarrier.pipelineStalls;
        operations.caches.u16All         |= m_currentBarrier.caches;

        RgpSqttMarkerBarrierEnd marker = {};

        VK_ASSERT(data.operations.layoutTransitions.u16All == 0);

        marker.identifier           = RgpSqttMarkerIdentifierBarrierEnd;
        marker.cbId                 = m_cbId.u32All;

        marker.waitOnEopTs          = (operations.pipelineStalls.eopTsBottomOfPipe &&
                                       operations.pipelineStalls.waitOnTs);
        marker.vsPartialFlush       = operations.pipelineStalls.vsPartialFlush;
        marker.psPartialFlush       = operations.pipelineStalls.psPartialFlush;
        marker.csPartialFlush       = operations.pipelineStalls.csPartialFlush;
        marker.pfpSyncMe            = operations.pipelineStalls.pfpSyncMe;
        marker.syncCpDma            = operations.pipelineStalls.syncCpDma;
        marker.invalTcp             = operations.caches.invalTcp;
        marker.invalSqI             = operations.caches.invalSqI$;
        marker.invalSqK             = operations.caches.invalSqK$;
        marker.flushTcc             = operations.caches.flushTcc;
        marker.invalTcc             = operations.caches.invalTcc;
        marker.flushCb              = operations.caches.flushCb;
        marker.invalCb              = operations.caches.invalCb;
        marker.flushDb              = operations.caches.flushDb;
        marker.invalDb              = operations.caches.invalDb;

        marker.numLayoutTransitions = m_currentBarrier.numLayoutTransitions;

        marker.invalGl1             = operations.caches.invalGl1;
        marker.waitOnTs             = operations.pipelineStalls.waitOnTs;
        marker.eopTsBottomOfPipe    = operations.pipelineStalls.eopTsBottomOfPipe;
        marker.eosTsPsDone          = operations.pipelineStalls.eosTsPsDone;
        marker.eosTsCsDone          = operations.pipelineStalls.eosTsCsDone;

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Inserts a command buffer start marker
void SqttCmdBufferState::WriteCbStartMarker() const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableCbStart)
    {
        RgpSqttMarkerCbStart marker = {};

        marker.identifier   = RgpSqttMarkerIdentifierCbStart;
        marker.cbID         = m_cbId.u32All;
        marker.deviceIdLow  = static_cast<uint32_t>(m_deviceId);
        marker.deviceIdHigh = static_cast<uint32_t>(m_deviceId >> 32);
        marker.queue        = m_queueFamilyIndex;
        marker.queueFlags   = m_queueFamilyFlags;

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Inserts a command buffer end marker
void SqttCmdBufferState::WriteCbEndMarker() const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableCbEnd)
    {
        RgpSqttMarkerCbEnd marker = {};

        marker.identifier   = RgpSqttMarkerIdentifierCbEnd;
        marker.cbID         = m_cbId.u32All;
        marker.deviceIdLow  = static_cast<uint32_t>(m_deviceId);
        marker.deviceIdHigh = static_cast<uint32_t>(m_deviceId >> 32);

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Inserts a pipeline bind marker
void SqttCmdBufferState::WritePipelineBindMarker(
    const Pal::Developer::BindPipelineData& data) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnablePipelineBind)
    {
        RgpSqttMarkerPipelineBind marker = {};

        marker.identifier = RgpSqttMarkerIdentifierBindPipeline;
        marker.cbID = m_cbId.u32All;

        switch (data.bindPoint)
        {
        case Pal::PipelineBindPoint::Compute:
            marker.bindPoint = 1;
            break;
        case Pal::PipelineBindPoint::Graphics:
            marker.bindPoint = 0;
            break;
        default:
            VK_NEVER_CALLED();
        }

        static_assert(sizeof(marker.apiPsoHash) == sizeof(data.apiPsoHash), "Api Pso Hash size mismatch");
        memcpy(marker.apiPsoHash, &data.apiPsoHash, sizeof(marker.apiPsoHash));

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Writes a general API marker at the top of the call
void SqttCmdBufferState::WriteBeginGeneralApiMarker(
    RgpSqttMarkerGeneralApiType apiType
    ) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableGeneralApi)
    {
        RgpSqttMarkerGeneralApi marker = {};

        marker.identifier = RgpSqttMarkerIdentifierGeneralApi;
        marker.apiType    = static_cast<uint32_t>(apiType);

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Writes a general API marker at the end of the call
void SqttCmdBufferState::WriteEndGeneralApiMarker(
    RgpSqttMarkerGeneralApiType apiType
    ) const
{
    if (m_enabledMarkers & DevModeSqttMarkerEnableGeneralApi)
    {
        RgpSqttMarkerGeneralApi marker = {};

        marker.identifier = RgpSqttMarkerIdentifierGeneralApi;
        marker.apiType    = static_cast<uint32_t>(apiType);
        marker.isEnd      = 1;

        Pal::RgpMarkerSubQueueFlags subQueueFlags = {};
        subQueueFlags.includeMainSubQueue         = 1;

        WriteMarker(&marker, sizeof(marker), subQueueFlags);
    }
}

// =====================================================================================================================
// Called when entering any SQTT function
void SqttCmdBufferState::BeginEntryPoint(RgpSqttMarkerGeneralApiType apiType)
{
    VK_ASSERT(m_currentEntryPoint == RgpSqttMarkerGeneralApiType::Invalid);

    if (apiType != RgpSqttMarkerGeneralApiType::Invalid)
    {
        WriteBeginGeneralApiMarker(apiType);

        m_currentEntryPoint = apiType;
    }
}

// =====================================================================================================================
// Called when leaving any SQTT function
void SqttCmdBufferState::EndEntryPoint()
{
    VK_ASSERT(m_currentEventType == RgpSqttMarkerEventType::InternalUnknown);

    if (m_currentEntryPoint != RgpSqttMarkerGeneralApiType::Invalid)
    {
        WriteEndGeneralApiMarker(m_currentEntryPoint);

        m_currentEntryPoint = RgpSqttMarkerGeneralApiType::Invalid;
    }
}

// =====================================================================================================================
// Called when a pipeline is bound
void SqttCmdBufferState::PipelineBound(
    VkPipelineBindPoint bindPoint,
    VkPipeline          pipeline)
{
    if (pipeline != VK_NULL_HANDLE)
    {
        const Pipeline* pPipeline = Pipeline::BaseObjectFromHandle(pipeline);

#if ICD_GPUOPEN_DEVMODE_BUILD
        if (m_pDevModeMgr != nullptr)
        {
            if ((m_instructionTrace.started == false) &&
                (pPipeline->GetApiHash() == m_instructionTrace.targetHash))
            {
                m_pDevModeMgr->StartInstructionTrace(m_pCmdBuf);
                m_instructionTrace.bindPoint = bindPoint;
                m_instructionTrace.started = true;
            }
        }
#endif

    }
}

// =====================================================================================================================
// Called prior to a render pass load-op color clear
void SqttCmdBufferState::BeginRenderPassColorClear()
{
    BeginEventMarkers(RgpSqttMarkerEventType::RenderPassColorClear);
}

// =====================================================================================================================
// Called after a render pass load-op color clear
void SqttCmdBufferState::EndRenderPassColorClear()
{
    VK_ASSERT(m_currentEventType == RgpSqttMarkerEventType::RenderPassColorClear);

    EndEventMarkers();
}

// =====================================================================================================================
// Called prior to a render pass load-op depth-stencil clear
void SqttCmdBufferState::BeginRenderPassDepthStencilClear()
{
    BeginEventMarkers(RgpSqttMarkerEventType::RenderPassDepthStencilClear);
}

// =====================================================================================================================
// Called after a render pass load-op depth-stencil clear
void SqttCmdBufferState::EndRenderPassDepthStencilClear()
{
    VK_ASSERT(m_currentEventType == RgpSqttMarkerEventType::RenderPassDepthStencilClear);

    EndEventMarkers();
}

// =====================================================================================================================
// Called prior to a render pass multisample resolve operation
void SqttCmdBufferState::BeginRenderPassResolve()
{
    BeginEventMarkers(RgpSqttMarkerEventType::RenderPassResolve);
}

// =====================================================================================================================
// Called after a render pass multisample resolve
void SqttCmdBufferState::EndRenderPassResolve()
{
    VK_ASSERT(m_currentEventType == RgpSqttMarkerEventType::RenderPassResolve);

    EndEventMarkers();
}

// =====================================================================================================================
void SqttCmdBufferState::DebugMarkerBegin(
    const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
    WriteUserEventMarker(RgpSqttMarkerUserEventPush, pMarkerInfo->pMarkerName);
}

// =====================================================================================================================
void SqttCmdBufferState::DebugMarkerEnd()
{
    WriteUserEventMarker(RgpSqttMarkerUserEventPop, nullptr);
}

// =====================================================================================================================
void SqttCmdBufferState::DebugMarkerInsert(
    const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
    WriteUserEventMarker(RgpSqttMarkerUserEventTrigger, pMarkerInfo->pMarkerName);
}

// =====================================================================================================================
void SqttCmdBufferState::DebugLabelBegin(
    const VkDebugUtilsLabelEXT* pMarkerInfo)
{
    WriteUserEventMarker(RgpSqttMarkerUserEventPush, pMarkerInfo->pLabelName);
}

// =====================================================================================================================
void SqttCmdBufferState::DebugLabelEnd()
{
    WriteUserEventMarker(RgpSqttMarkerUserEventPop, nullptr);
}

// =====================================================================================================================
void SqttCmdBufferState::DebugLabelInsert(
    const VkDebugUtilsLabelEXT* pMarkerInfo)
{
    WriteUserEventMarker(RgpSqttMarkerUserEventTrigger, pMarkerInfo->pLabelName);
}

// =====================================================================================================================
void SqttCmdBufferState::AddDebugTag(uint64_t tag)
{
    if (HasDebugTag(tag) == false)
    {
        m_debugTags.PushBack(tag);
    }
}

// =====================================================================================================================
bool SqttCmdBufferState::HasDebugTag(
    uint64_t tag
    ) const
{
    auto it = m_debugTags.Begin();

    while (it.Get() != nullptr)
    {
        if (tag == *it.Get())
        {
            return true;
        }

        it.Next();
    }

    return false;
}

// =====================================================================================================================
SqttCmdBufferState::~SqttCmdBufferState()
{

}

namespace entry
{

namespace sqtt
{

// Helper macro that sets up some common local variables and does marker work common to the start of each vkCmd*
// function
#define SQTT_SETUP() \
    SqttCmdBufferState* pSqtt = ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->GetSqttState(); \

// Helper function to call the next layer's function by name
#define SQTT_CALL_NEXT_LAYER(entry_name) \
    pSqtt->GetNextLayer()->GetEntryPoints().entry_name

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBindPipeline);

    pSqtt->PipelineBound(pipelineBindPoint, pipeline);

    SQTT_CALL_NEXT_LAYER(vkCmdBindPipeline)(cmdBuffer, pipelineBindPoint, pipeline);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBindDescriptorSets);

    SQTT_CALL_NEXT_LAYER(vkCmdBindDescriptorSets)(cmdBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount,
        pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBindIndexBuffer);

    SQTT_CALL_NEXT_LAYER(vkCmdBindIndexBuffer)(cmdBuffer, buffer, offset, indexType);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBindVertexBuffers);

    SQTT_CALL_NEXT_LAYER(vkCmdBindVertexBuffers)(cmdBuffer, firstBinding, bindingCount, pBuffers, pOffsets);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDraw);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDraw);

    SQTT_CALL_NEXT_LAYER(vkCmdDraw)(cmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndexed);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndexed);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndexed)(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset,
        firstInstance);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndirect);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndirect);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndirect)(cmdBuffer, buffer, offset, drawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndexedIndirect);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndexedIndirect);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndexedIndirect)(cmdBuffer, buffer, offset, drawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountAMD(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndirectCountAMD);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndirectCountAMD);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndirectCountAMD)(cmdBuffer, buffer, offset, countBuffer, countOffset, maxDrawCount,
        stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountAMD(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndexedIndirectCountAMD);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndexedIndirectCountAMD);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndexedIndirectCountAMD)(cmdBuffer, buffer, offset, countBuffer, countOffset,
        maxDrawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountKHR(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndirectCountKHR);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndirectCountKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndirectCountKHR)(cmdBuffer, buffer, offset, countBuffer, countOffset, maxDrawCount,
        stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountKHR(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawIndexedIndirectCountKHR);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawIndexedIndirectCountKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawIndexedIndirectCountKHR)(cmdBuffer, buffer, offset, countBuffer, countOffset,
        maxDrawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksEXT(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawMeshTasksEXT);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawMeshTasksEXT);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawMeshTasksEXT)(cmdBuffer, groupCountX, groupCountY, groupCountZ);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectEXT(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawMeshTasksIndirectEXT);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawMeshTasksIndirectEXT);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawMeshTasksIndirectEXT)(cmdBuffer, buffer, offset, drawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectCountEXT(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDrawMeshTasksIndirectCountEXT);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDrawMeshTasksIndirectCountEXT);

    SQTT_CALL_NEXT_LAYER(vkCmdDrawMeshTasksIndirectCountEXT)(cmdBuffer, buffer, offset, countBuffer, countBufferOffset,
        maxDrawCount, stride);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    x,
    uint32_t                                    y,
    uint32_t                                    z)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDispatch);

    SQTT_CALL_NEXT_LAYER(vkCmdDispatch)(cmdBuffer, x, y, z);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatchIndirect);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdDispatchIndirect);

    SQTT_CALL_NEXT_LAYER(vkCmdDispatchIndirect)(cmdBuffer, buffer, offset);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferCopy*                         pRegions)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdCopyBuffer);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyBuffer);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyBuffer)(cmdBuffer, srcBuffer, dstBuffer, regionCount, pRegions);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageCopy*                          pRegions)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdCopyImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyImage);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyImage)(cmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
        pRegions);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageBlit*                          pRegions,
    VkFilter                                    filter)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBlitImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdBlitImage);

    SQTT_CALL_NEXT_LAYER(vkCmdBlitImage)(cmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
        pRegions, filter);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdCopyBufferToImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyBufferToImage);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyBufferToImage)(cmdBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdCopyImageToBuffer);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyImageToBuffer);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyImageToBuffer)(cmdBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdUpdateBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                dataSize,
    const void*                                 pData)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdUpdateBuffer);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdUpdateBuffer);

    SQTT_CALL_NEXT_LAYER(vkCmdUpdateBuffer)(cmdBuffer, dstBuffer, dstOffset, dataSize, pData);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                size,
    uint32_t                                    data)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdFillBuffer);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdFillBuffer);

    SQTT_CALL_NEXT_LAYER(vkCmdFillBuffer)(cmdBuffer, dstBuffer, dstOffset, size, data);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearColorImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdClearColorImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdClearColorImage);

    SQTT_CALL_NEXT_LAYER(vkCmdClearColorImage)(cmdBuffer, image, imageLayout, pColor, rangeCount, pRanges);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearDepthStencilImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearDepthStencilValue*             pDepthStencil,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdClearDepthStencilImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdClearDepthStencilImage);

    SQTT_CALL_NEXT_LAYER(vkCmdClearDepthStencilImage)(cmdBuffer, image, imageLayout, pDepthStencil, rangeCount,
        pRanges);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    attachmentCount,
    const VkClearAttachment*                    pAttachments,
    uint32_t                                    rectCount,
    const VkClearRect*                          pRects)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdClearAttachments);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdClearAttachments);

    SQTT_CALL_NEXT_LAYER(vkCmdClearAttachments)(cmdBuffer, attachmentCount, pAttachments, rectCount, pRects);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageResolve*                       pRegions)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdResolveImage);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdResolveImage);

    SQTT_CALL_NEXT_LAYER(vkCmdResolveImage)(cmdBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount,
        pRegions);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdWaitEvents);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdWaitEvents);

    SQTT_CALL_NEXT_LAYER(vkCmdWaitEvents)(cmdBuffer, eventCount, pEvents, srcStageMask, dstStageMask,
        memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount,
        pImageMemoryBarriers);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    VkDependencyFlags                           dependencyFlags,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdPipelineBarrier);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdPipelineBarrier);

    SQTT_CALL_NEXT_LAYER(vkCmdPipelineBarrier)(cmdBuffer, srcStageMask, dstStageMask, dependencyFlags,
        memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount,
        pImageMemoryBarriers);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginQuery(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    VkQueryControlFlags                         flags)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBeginQuery);

    SQTT_CALL_NEXT_LAYER(vkCmdBeginQuery)(cmdBuffer, queryPool, query, flags);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdEndQuery);

    SQTT_CALL_NEXT_LAYER(vkCmdEndQuery)(cmdBuffer, queryPool, query);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdResetQueryPool);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdResetQueryPool);

    SQTT_CALL_NEXT_LAYER(vkCmdResetQueryPool)(cmdBuffer, queryPool, firstQuery, queryCount);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineStageFlagBits                     pipelineStage,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdWriteTimestamp);

    SQTT_CALL_NEXT_LAYER(vkCmdWriteTimestamp)(cmdBuffer, pipelineStage, queryPool, query);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyQueryPoolResults(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                stride,
    VkQueryResultFlags                          flags)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdCopyQueryPoolResults);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyQueryPoolResults);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyQueryPoolResults)(cmdBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset,
        stride, flags);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    offset,
    uint32_t                                    size,
    const void*                                 pValues)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdPushConstants);

    SQTT_CALL_NEXT_LAYER(vkCmdPushConstants)(cmdBuffer, layout, stageFlags, offset, size, pValues);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer                             cmdBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBeginRenderPass);

    SQTT_CALL_NEXT_LAYER(vkCmdBeginRenderPass)(cmdBuffer, pRenderPassBegin, contents);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer                             cmdBuffer,
    VkSubpassContents                           contents)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdNextSubpass);

    SQTT_CALL_NEXT_LAYER(vkCmdNextSubpass)(cmdBuffer, contents);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             cmdBuffer)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdEndRenderPass);

    SQTT_CALL_NEXT_LAYER(vkCmdEndRenderPass)(cmdBuffer);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdExecuteCommands);

    SQTT_CALL_NEXT_LAYER(vkCmdExecuteCommands)(cmdBuffer, commandBufferCount, pCommandBuffers);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstViewport,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetViewport);

    SQTT_CALL_NEXT_LAYER(vkCmdSetViewport)(cmdBuffer, firstViewport, viewportCount, pViewports);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetScissor);

    SQTT_CALL_NEXT_LAYER(vkCmdSetScissor)(cmdBuffer, firstScissor, scissorCount, pScissors);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineWidth(
    VkCommandBuffer                             cmdBuffer,
    float                                       lineWidth)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetLineWidth);

    SQTT_CALL_NEXT_LAYER(vkCmdSetLineWidth)(cmdBuffer, lineWidth);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBias(
    VkCommandBuffer                             cmdBuffer,
    float                                       depthBiasConstantFactor,
    float                                       depthBiasClamp,
    float                                       depthBiasSlopeFactor)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetDepthBias);

    SQTT_CALL_NEXT_LAYER(vkCmdSetDepthBias)(cmdBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetBlendConstants(
    VkCommandBuffer                             cmdBuffer,
    const float                                 blendConstants[4])
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetBlendConstants);

    SQTT_CALL_NEXT_LAYER(vkCmdSetBlendConstants)(cmdBuffer, blendConstants);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBounds(
    VkCommandBuffer                             cmdBuffer,
    float                                       minDepthBounds,
    float                                       maxDepthBounds)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetDepthBounds);

    SQTT_CALL_NEXT_LAYER(vkCmdSetDepthBounds)(cmdBuffer, minDepthBounds, maxDepthBounds);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilCompareMask(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    compareMask)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetStencilCompareMask);

    SQTT_CALL_NEXT_LAYER(vkCmdSetStencilCompareMask)(cmdBuffer, faceMask, compareMask);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilWriteMask(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    writeMask)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetStencilWriteMask);

    SQTT_CALL_NEXT_LAYER(vkCmdSetStencilWriteMask)(cmdBuffer, faceMask, writeMask);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilReference(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    reference)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdSetStencilReference);

    SQTT_CALL_NEXT_LAYER(vkCmdSetStencilReference)(cmdBuffer, faceMask, reference);

    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
{
    const VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugMarkerBegin(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkCmdDebugMarkerBeginEXT)(commandBuffer, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT(
    VkCommandBuffer                             commandBuffer)
{
    VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugMarkerEnd();

    SQTT_CALL_NEXT_LAYER(vkCmdDebugMarkerEndEXT)(commandBuffer);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
{
    VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugMarkerInsert(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkCmdDebugMarkerInsertEXT)(commandBuffer, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pMarkerInfo)
{
    const VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugLabelBegin(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkCmdBeginDebugUtilsLabelEXT)(commandBuffer, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer)
{
    VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugLabelEnd();

    SQTT_CALL_NEXT_LAYER(vkCmdEndDebugUtilsLabelEXT)(commandBuffer);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pMarkerInfo)
{
    VkCommandBuffer cmdBuffer = commandBuffer;
    SQTT_SETUP();

    pSqtt->DebugLabelInsert(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkCmdInsertDebugUtilsLabelEXT)(commandBuffer, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pMarkerInfo)
{
    SqttQueueState* pSqtt = ApiQueue::ObjectFromHandle(queue)->GetSqttState();

    pSqtt->DebugLabelBegin(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkQueueBeginDebugUtilsLabelEXT)(queue, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(
    VkQueue                                     queue)
{
    SqttQueueState* pSqtt = ApiQueue::ObjectFromHandle(queue)->GetSqttState();

    pSqtt->DebugLabelEnd();

    SQTT_CALL_NEXT_LAYER(vkQueueEndDebugUtilsLabelEXT)(queue);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
    VkQueue                                     queue,
    const VkDebugUtilsLabelEXT*                 pMarkerInfo)
{
    SqttQueueState* pSqtt = ApiQueue::ObjectFromHandle(queue)->GetSqttState();

    pSqtt->DebugLabelInsert(pMarkerInfo);

    SQTT_CALL_NEXT_LAYER(vkQueueInsertDebugUtilsLabelEXT)(queue, pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkGraphicsPipelineCreateInfo*         pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device* pDevice     = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt      = pDevice->GetSqttMgr();
    DevModeMgr* pDevMgr = pDevice->VkInstance()->GetDevModeMgr();

    VkResult result = SQTT_CALL_NEXT_LAYER(vkCreateGraphicsPipelines)(device, pipelineCache, createInfoCount,
                                                                      pCreateInfos, pAllocator, pPipelines);

    if (pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (result == VK_SUCCESS) &&
        (pDevMgr != nullptr))
    {
        for (uint32_t i = 0; i < createInfoCount; ++i)
        {
            if (pPipelines[i] != VK_NULL_HANDLE)
            {
                GraphicsPipeline* pPipeline = NonDispatchable<VkPipeline, GraphicsPipeline>::ObjectFromHandle(
                    pPipelines[i]);

                auto* pMeta = pSqtt->GetObjectMgr()->ObjectCreated(pDevice,
                    VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pPipelines[i]);

                if (pMeta != nullptr)
                {
                    memset(pMeta->pipeline.shaderModules, 0, sizeof(pMeta->pipeline.shaderModules));

                    for (uint32_t stage = 0; stage < pCreateInfos[i].stageCount; ++stage)
                    {
                        size_t palIdx = static_cast<size_t>(VkToPalShaderType(pCreateInfos[i].pStages[stage].stage));

                        pMeta->pipeline.shaderModules[palIdx] = pCreateInfos[i].pStages[stage].module;
                    }
                }

#if ICD_GPUOPEN_DEVMODE_BUILD
                pDevMgr->PipelineCreated(pDevice, pPipeline);
#endif
            }
        }
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(
    VkDevice                                    device,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkComputePipelineCreateInfo*          pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device* pDevice     = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt      = pDevice->GetSqttMgr();
    DevModeMgr* pDevMgr = pDevice->VkInstance()->GetDevModeMgr();

    VkResult result = SQTT_CALL_NEXT_LAYER(vkCreateComputePipelines)(device, pipelineCache, createInfoCount,
        pCreateInfos, pAllocator, pPipelines);

    if (pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        (result == VK_SUCCESS) &&
        (pDevMgr != nullptr))
    {
        for (uint32_t i = 0; i < createInfoCount; ++i)
        {
            if (pPipelines[i] != VK_NULL_HANDLE)
            {
                ComputePipeline* pPipeline = NonDispatchable<VkPipeline, ComputePipeline>::ObjectFromHandle(
                    pPipelines[i]);

                auto* pMeta = pSqtt->GetObjectMgr()->ObjectCreated(pDevice,
                    VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pPipelines[i]);

                if (pMeta != nullptr)
                {
                    memset(pMeta->pipeline.shaderModules, 0, sizeof(pMeta->pipeline.shaderModules));

                    pMeta->pipeline.shaderModules[static_cast<size_t>(Pal::ShaderType::Compute)] =
                        pCreateInfos[i].stage.module;
                }

#if ICD_GPUOPEN_DEVMODE_BUILD
                pDevMgr->PipelineCreated(pDevice, pPipeline);
#endif
            }
        }
    }

    return result;
}

#if VKI_RAY_TRACING
// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(
    VkDevice                                    device,
    VkDeferredOperationKHR                      deferredOperation,
    VkPipelineCache                             pipelineCache,
    uint32_t                                    createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR*    pCreateInfos,
    const VkAllocationCallbacks*                pAllocator,
    VkPipeline*                                 pPipelines)
{
    Device* pDevice     = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt      = pDevice->GetSqttMgr();
    DevModeMgr* pDevMgr = pDevice->VkInstance()->GetDevModeMgr();

    VkResult result = SQTT_CALL_NEXT_LAYER(vkCreateRayTracingPipelinesKHR)(device, deferredOperation, pipelineCache,
        createInfoCount, pCreateInfos, pAllocator, pPipelines);

    if (pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable &&
        ((result == VK_SUCCESS) || (result == VK_OPERATION_DEFERRED_KHR)) &&
        (pDevMgr != nullptr))
    {
        for (uint32_t i = 0; i < createInfoCount; ++i)
        {
            if (pPipelines[i] != VK_NULL_HANDLE)
            {
                RayTracingPipeline* pPipeline = NonDispatchable<VkPipeline, RayTracingPipeline>::ObjectFromHandle(
                    pPipelines[i]);

                auto* pMeta = pSqtt->GetObjectMgr()->ObjectCreated(pDevice,
                    VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, pPipelines[i]);

                if (pMeta != nullptr)
                {
                    memset(pMeta->pipeline.shaderModules, 0, sizeof(pMeta->pipeline.shaderModules));

                    for (uint32_t stage = 0; stage < pCreateInfos[i].stageCount; ++stage)
                    {
                        size_t palIdx = static_cast<size_t>(VkToPalShaderType(pCreateInfos[i].pStages[stage].stage));

                        pMeta->pipeline.shaderModules[palIdx] = pCreateInfos[i].pStages[stage].module;
                    }
                }

#if ICD_GPUOPEN_DEVMODE_BUILD
                if (result != VK_OPERATION_DEFERRED_KHR)
                {
                    pDevMgr->PipelineCreated(pDevice, pPipeline);

                    if (pPipeline->IsInlinedShaderEnabled() == false)
                    {
                        pDevMgr->ShaderLibrariesCreated(pDevice, pPipeline);
                    }
                }
#endif
            }
        }
    }

    return result;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer                             cmdBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth)
{
    SQTT_SETUP();

    RgpSqttMarkerEventType eventType = RgpSqttMarkerEventType::CmdTraceRaysKHR;
    if (ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->GetBoundRayTracingPipeline()->IsInlinedShaderEnabled() == false)
    {
        eventType |= RgpSqttMarkerEventType::ShaderIndirectModeMask;
    }

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(eventType);

    SQTT_CALL_NEXT_LAYER(vkCmdTraceRaysKHR)(cmdBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable,
        pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirectKHR(
    VkCommandBuffer                             cmdBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    VkDeviceAddress                             indirectDeviceAddress)
{
    SQTT_SETUP();

    RgpSqttMarkerEventType eventType = RgpSqttMarkerEventType::CmdTraceRaysIndirectKHR;
    if (ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->GetBoundRayTracingPipeline()->IsInlinedShaderEnabled() == false)
    {
        eventType |= RgpSqttMarkerEventType::ShaderIndirectModeMask;
    }

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(eventType);

    SQTT_CALL_NEXT_LAYER(vkCmdTraceRaysIndirectKHR)(cmdBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable,
                                            pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer                                         cmdBuffer,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdBuildAccelerationStructuresKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresKHR)(cmdBuffer, infoCount, pInfos, ppBuildRangeInfos);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer                                    cmdBuffer,
    uint32_t                                           infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkDeviceAddress*                             pIndirectDeviceAddresses,
    const uint32_t*                                    pIndirectStrides,
    const uint32_t* const*                             ppMaxPrimitiveCounts)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdBuildAccelerationStructuresIndirectKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdBuildAccelerationStructuresIndirectKHR)(cmdBuffer, infoCount, pInfos,
        pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureKHR(
    VkCommandBuffer                             cmdBuffer,
    const VkCopyAccelerationStructureInfoKHR*   pInfo)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyAccelerationStructureKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyAccelerationStructureKHR)(cmdBuffer, pInfo);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer                                     cmdBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR*   pInfo)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyAccelerationStructureToMemoryKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyAccelerationStructureToMemoryKHR)(cmdBuffer, pInfo);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer                                     cmdBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR*   pInfo)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdDispatch);
    pSqtt->BeginEventMarkers(RgpSqttMarkerEventType::CmdCopyMemoryToAccelerationStructureKHR);

    SQTT_CALL_NEXT_LAYER(vkCmdCopyMemoryToAccelerationStructureKHR)(cmdBuffer, pInfo);

    pSqtt->EndEventMarkers();
    pSqtt->EndEntryPoint();
}
#endif

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    const VkAllocationCallbacks*                pAllocator)
{
    Device* pDevice     = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt      = pDevice->GetSqttMgr();
    DevModeMgr* pDevMgr = pDevice->VkInstance()->GetDevModeMgr();

#if ICD_GPUOPEN_DEVMODE_BUILD
    if (pDevice->GetRuntimeSettings().devModeShaderIsaDbEnable && (pDevMgr != nullptr))
    {
        if (VK_NULL_HANDLE != pipeline)
        {
            Pipeline* pPipeline = Pipeline::BaseObjectFromHandle(pipeline);

            pDevMgr->PipelineDestroyed(pDevice, pPipeline);

#if VKI_RAY_TRACING
            if (pPipeline->GetType() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
            {
                RayTracingPipeline* pRtPipeline = RayTracingPipeline::ObjectFromHandle(pipeline);

                if (pRtPipeline->IsInlinedShaderEnabled() == false)
                {
                    pDevMgr->ShaderLibrariesDestroyed(pDevice, pRtPipeline);
                }
            }
#endif
        }
    }
#endif

    return SQTT_CALL_NEXT_LAYER(vkDestroyPipeline)(device, pipeline, pAllocator);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectNameInfoEXT*       pNameInfo)
{
    Device* pDevice  = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt   = pDevice->GetSqttMgr();
    auto* pObjectMgr = pSqtt->GetObjectMgr();

    if (pObjectMgr->IsEnabled(pNameInfo->objectType))
    {
        SqttMetaState* pMeta = pObjectMgr->GetMetaState(pNameInfo->objectType, pNameInfo->object);

        if (pMeta == nullptr)
        {
            pObjectMgr->ObjectCreated(pDevice, pNameInfo->objectType, pNameInfo->object);

            pMeta = pObjectMgr->GetMetaState(pNameInfo->objectType, pNameInfo->object);
        }

        if (pMeta != nullptr)
        {
            size_t nameSize = strlen(pNameInfo->pObjectName) + 1;

            if (pMeta->debugNameCapacity < nameSize)
            {
                if (pMeta->pDebugName != nullptr)
                {
                    pDevice->VkInstance()->FreeMem(pMeta->pDebugName);
                }

                pMeta->pDebugName = static_cast<char*>(pDevice->VkInstance()->AllocMem(nameSize,
                    VK_SYSTEM_ALLOCATION_SCOPE_DEVICE));

                if (pMeta->pDebugName != nullptr)
                {
                    pMeta->debugNameCapacity = nameSize;
                }
            }

            if ((pMeta->debugNameCapacity >= nameSize) && (pMeta->pDebugName != nullptr))
            {
                memcpy(pMeta->pDebugName, pNameInfo->pObjectName, nameSize);
            }
        }
    }

    return SQTT_CALL_NEXT_LAYER(vkDebugMarkerSetObjectNameEXT)(device, pNameInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT(
    VkDevice                                    device,
    const VkDebugMarkerObjectTagInfoEXT*        pTagInfo)
{
    Device* pDevice   = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt    = pDevice->GetSqttMgr();

    if ((pTagInfo != nullptr) &&
        (pTagInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT) &&
        (pTagInfo->object != 0))
    {
        SqttCmdBufferState* pCmdBuf = ApiCmdBuffer::ObjectFromHandle(VkCommandBuffer(pTagInfo->object))->GetSqttState();

        pCmdBuf->AddDebugTag(pTagInfo->tagName);
    }

    return SQTT_CALL_NEXT_LAYER(vkDebugMarkerSetObjectTagEXT)(device, pTagInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectNameInfoEXT*        pNameInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt = pDevice->GetSqttMgr();
    auto* pObjectMgr = pSqtt->GetObjectMgr();

    if (pObjectMgr->IsEnabled(pNameInfo->objectType))
    {
        SqttMetaState* pMeta = pObjectMgr->GetMetaState(pNameInfo->objectType, pNameInfo->objectHandle);

        if (pMeta == nullptr)
        {
            pObjectMgr->ObjectCreated(pDevice, pNameInfo->objectType, pNameInfo->objectHandle);

            pMeta = pObjectMgr->GetMetaState(pNameInfo->objectType, pNameInfo->objectHandle);
        }

        if (pMeta != nullptr)
        {
            size_t nameSize = strlen(pNameInfo->pObjectName) + 1;

            if (pMeta->debugNameCapacity < nameSize)
            {
                if (pMeta->pDebugName != nullptr)
                {
                    pDevice->VkInstance()->FreeMem(pMeta->pDebugName);
                }

                pMeta->pDebugName = static_cast<char*>(pDevice->VkInstance()->AllocMem(nameSize,
                    VK_SYSTEM_ALLOCATION_SCOPE_DEVICE));

                if (pMeta->pDebugName != nullptr)
                {
                    pMeta->debugNameCapacity = nameSize;
                }
            }

            if ((pMeta->debugNameCapacity >= nameSize) && (pMeta->pDebugName != nullptr))
            {
                memcpy(pMeta->pDebugName, pNameInfo->pObjectName, nameSize);
            }
        }
    }

    return SQTT_CALL_NEXT_LAYER(vkSetDebugUtilsObjectNameEXT)(device, pNameInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectTagEXT(
    VkDevice                                    device,
    const VkDebugUtilsObjectTagInfoEXT*         pTagInfo)
{
    Device* pDevice = ApiDevice::ObjectFromHandle(device);
    SqttMgr* pSqtt = pDevice->GetSqttMgr();

    if ((pTagInfo != nullptr) &&
        (pTagInfo->objectType == VK_OBJECT_TYPE_COMMAND_BUFFER) &&
        (pTagInfo->objectHandle != 0))
    {
        SqttCmdBufferState* pCmdBuf = ApiCmdBuffer::ObjectFromHandle(VkCommandBuffer(pTagInfo->objectHandle))->GetSqttState();

        pCmdBuf->AddDebugTag(pTagInfo->tagName);
    }

    return SQTT_CALL_NEXT_LAYER(vkSetDebugUtilsObjectTagEXT)(device, pTagInfo);
}

#if ICD_GPUOPEN_DEVMODE_BUILD
// =====================================================================================================================
// This function looks for specific tags in a submit's command buffers to identify when to force an RGP trace start
// rather than during it during vkQueuePresent().  This is done for applications that explicitly do not make present
// calls but still want to start/stop RGP tracing.
static void CheckRGPFrameBegin(
    Queue*              pQueue,
    DevModeMgr*         pDevMode,
    uint32_t            submitCount,
    const VkSubmitInfo* pSubmits)
{
    uint64_t frameBeginTag;

    // Check with developer mode whether there's a valid frame begin tag.  If there is, a trace is in progress
    // and we need to check for matching command buffer tags in this submit.  If there's a match, notify
    // developer mode of a frame begin boundary.
    if (pDevMode->GetTraceFrameBeginTag(&frameBeginTag))
    {
        for (uint32_t si = 0; si < submitCount; ++si)
        {
            const VkSubmitInfo& submitInfo = pSubmits[si];

            for (uint32_t ci = 0; ci < submitInfo.commandBufferCount; ++ci)
            {
                const SqttCmdBufferState* pCmdBuf =
                    ApiCmdBuffer::ObjectFromHandle(submitInfo.pCommandBuffers[ci])->GetSqttState();

                if (pCmdBuf->HasDebugTag(frameBeginTag))
                {
                    pDevMode->NotifyFrameBegin(pQueue, DevModeMgr::FrameDelimiterType::CmdBufferTag);

                    return;
                }
            }
        }
    }
}

// =====================================================================================================================
// Looks for markers in a submitted command buffer to identify a forced end to an RGP trace.  See CheckRGPFrameBegin().
static void CheckRGPFrameEnd(
    Queue*              pQueue,
    DevModeMgr*         pDevMode,
    uint32_t            submitCount,
    const VkSubmitInfo* pSubmits)
{
    uint64_t frameEndTag;

    // Check with developer mode whether there's a valid frame end tag.  If there is, a trace is in progress
    // and we need to check for matching command buffer tags in this submit.  If there's a match, notify
    // developer mode of a frame end boundary.
    if (pDevMode->GetTraceFrameEndTag(&frameEndTag))
    {
        for (uint32_t si = 0; si < submitCount; ++si)
        {
            const VkSubmitInfo& submitInfo = pSubmits[si];

            for (uint32_t ci = 0; ci < submitInfo.commandBufferCount; ++ci)
            {
                const SqttCmdBufferState* pCmdBuf =
                    ApiCmdBuffer::ObjectFromHandle(submitInfo.pCommandBuffers[ci])->GetSqttState();

                if (pCmdBuf->HasDebugTag(frameEndTag))
                {
                    pDevMode->NotifyFrameEnd(pQueue, DevModeMgr::FrameDelimiterType::CmdBufferTag);

                    return;
                }
            }
        }
    }
}
#endif

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue                                     queue,
    uint32_t                                    submitCount,
    const VkSubmitInfo*                         pSubmits,
    VkFence                                     fence)
{
    Queue* pQueue        = ApiQueue::ObjectFromHandle(queue);
    SqttMgr* pSqtt       = pQueue->VkDevice()->GetSqttMgr();
    DevModeMgr* pDevMode = pQueue->VkDevice()->VkInstance()->GetDevModeMgr();

#if ICD_GPUOPEN_DEVMODE_BUILD
    pDevMode->NotifyPreSubmit();

    CheckRGPFrameBegin(pQueue, pDevMode, submitCount, pSubmits);
#endif

    VkResult result = SQTT_CALL_NEXT_LAYER(vkQueueSubmit)(queue, submitCount, pSubmits, fence);

#if ICD_GPUOPEN_DEVMODE_BUILD
    CheckRGPFrameEnd(pQueue, pDevMode, submitCount, pSubmits);
#endif

    return result;
}

} // namespace sqtt

} // namespace entry

#define SQTT_OVERRIDE_ALIAS(entry_name, func_name) \
    if (pDispatchTable->entry_name##_condition) \
    { \
        pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::sqtt::func_name; \
    }

#define SQTT_OVERRIDE_ENTRY(entry_name) SQTT_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void SqttOverrideDispatchTable(
    DispatchTable*  pDispatchTable,
    SqttMgr*        pMgr)
{
    if (pMgr != nullptr)
    {
        // If a manager already exists (created together with the device) then we have to take a snapshot of the
        // device dispatch table that we'll use as the next level layer.
        pMgr->SaveNextLayer();
    }

    // Always override the necessary entry points, no matter if we're patching the device or the instance dispatch
    // table.
    SQTT_OVERRIDE_ENTRY(vkCmdBindPipeline);
    SQTT_OVERRIDE_ENTRY(vkCmdBindDescriptorSets);
    SQTT_OVERRIDE_ENTRY(vkCmdBindIndexBuffer);
    SQTT_OVERRIDE_ENTRY(vkCmdBindVertexBuffers);
    SQTT_OVERRIDE_ENTRY(vkCmdDraw);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndexed);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndirect);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndexedIndirect);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndirectCountAMD);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndexedIndirectCountAMD);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndirectCountKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawIndexedIndirectCountKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawMeshTasksEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawMeshTasksIndirectCountEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdDrawMeshTasksIndirectEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdDispatch);
    SQTT_OVERRIDE_ENTRY(vkCmdDispatchIndirect);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyBuffer);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyImage);
    SQTT_OVERRIDE_ENTRY(vkCmdBlitImage);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyBufferToImage);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyImageToBuffer);
    SQTT_OVERRIDE_ENTRY(vkCmdUpdateBuffer);
    SQTT_OVERRIDE_ENTRY(vkCmdFillBuffer);
    SQTT_OVERRIDE_ENTRY(vkCmdClearColorImage);
    SQTT_OVERRIDE_ENTRY(vkCmdClearDepthStencilImage);
    SQTT_OVERRIDE_ENTRY(vkCmdClearAttachments);
    SQTT_OVERRIDE_ENTRY(vkCmdResolveImage);
    SQTT_OVERRIDE_ENTRY(vkCmdWaitEvents);
    SQTT_OVERRIDE_ENTRY(vkCmdPipelineBarrier);
    SQTT_OVERRIDE_ENTRY(vkCmdBeginQuery);
    SQTT_OVERRIDE_ENTRY(vkCmdEndQuery);
    SQTT_OVERRIDE_ENTRY(vkCmdResetQueryPool);
    SQTT_OVERRIDE_ENTRY(vkCmdWriteTimestamp);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyQueryPoolResults);
    SQTT_OVERRIDE_ENTRY(vkCmdPushConstants);
    SQTT_OVERRIDE_ENTRY(vkCmdBeginRenderPass);
    SQTT_OVERRIDE_ENTRY(vkCmdNextSubpass);
    SQTT_OVERRIDE_ENTRY(vkCmdEndRenderPass);
    SQTT_OVERRIDE_ENTRY(vkCmdExecuteCommands);
    SQTT_OVERRIDE_ENTRY(vkCmdSetViewport);
    SQTT_OVERRIDE_ENTRY(vkCmdSetScissor);
    SQTT_OVERRIDE_ENTRY(vkCmdSetLineWidth);
    SQTT_OVERRIDE_ENTRY(vkCmdSetDepthBias);
    SQTT_OVERRIDE_ENTRY(vkCmdSetBlendConstants);
    SQTT_OVERRIDE_ENTRY(vkCmdSetDepthBounds);
    SQTT_OVERRIDE_ENTRY(vkCmdSetStencilCompareMask);
    SQTT_OVERRIDE_ENTRY(vkCmdSetStencilWriteMask);
    SQTT_OVERRIDE_ENTRY(vkCmdSetStencilReference);
    SQTT_OVERRIDE_ENTRY(vkCmdDebugMarkerBeginEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdDebugMarkerEndEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdDebugMarkerInsertEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdBeginDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdEndDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkCmdInsertDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkQueueBeginDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkQueueEndDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkQueueInsertDebugUtilsLabelEXT);
    SQTT_OVERRIDE_ENTRY(vkCreateGraphicsPipelines);
    SQTT_OVERRIDE_ENTRY(vkCreateComputePipelines);
#if VKI_RAY_TRACING
    SQTT_OVERRIDE_ENTRY(vkCreateRayTracingPipelinesKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdTraceRaysKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdTraceRaysIndirectKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdBuildAccelerationStructuresKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdBuildAccelerationStructuresIndirectKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyAccelerationStructureKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyAccelerationStructureToMemoryKHR);
    SQTT_OVERRIDE_ENTRY(vkCmdCopyMemoryToAccelerationStructureKHR);
#endif
    SQTT_OVERRIDE_ENTRY(vkDestroyPipeline);
    SQTT_OVERRIDE_ENTRY(vkDebugMarkerSetObjectNameEXT);
    SQTT_OVERRIDE_ENTRY(vkDebugMarkerSetObjectTagEXT);
    SQTT_OVERRIDE_ENTRY(vkSetDebugUtilsObjectNameEXT);
    SQTT_OVERRIDE_ENTRY(vkSetDebugUtilsObjectTagEXT);
    SQTT_OVERRIDE_ENTRY(vkQueueSubmit);
}

} // namespace vk
