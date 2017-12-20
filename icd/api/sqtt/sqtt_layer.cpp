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
 * @file  sqtt_layer.cpp
 * @brief Implementation of the SQTT layer.  This layer is an internal driver layer (not true loader-aware layer)
 *        that intercepts certain API calls to insert metadata tokens into the command stream while SQ thread tracing
 *        is active for the purposes of developer mode RGP profiling.
 ***********************************************************************************************************************
 */

#include "devmode/devmode_mgr.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_physical_device.h"
#include "include/vk_queue.h"
#include "include/vk_instance.h"
#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

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

// =====================================================================================================================
// Initialize per-cmdbuf SQTT layer info
SqttCmdBufferState::SqttCmdBufferState(
    CmdBuffer* pCmdBuf)
    :
    m_pCmdBuf(pCmdBuf),
    m_pSqttMgr(pCmdBuf->VkDevice()->GetSqttMgr()),
    m_settings(pCmdBuf->VkDevice()->GetRuntimeSettings()),
    m_pNextLayer(m_pSqttMgr->GetNextLayer()),
    m_currentEntryPoint(RgpSqttMarkerGeneralApiType::Invalid),
    m_currentEventId(0),
    m_currentEventType(RgpSqttMarkerEventType::InternalUnknown)
{
    m_cbId.u32All       = 0;
    m_deviceId          = reinterpret_cast<uint64_t>(ApiDevice::FromObject(m_pCmdBuf->VkDevice()));
    m_queueFamilyIndex  = m_pCmdBuf->GetQueueFamilyIndex();

    uint32_t queueCount = Queue::MaxQueueFamilies;
    VkQueueFamilyProperties queueProps[Queue::MaxQueueFamilies] = {};

    VkResult result = m_pCmdBuf->VkDevice()->VkPhysicalDevice()->GetQueueFamilyProperties(&queueCount, queueProps);

    VK_ASSERT(result == VK_SUCCESS);
    VK_ASSERT(m_queueFamilyIndex < queueCount);

    m_queueFamilyFlags = queueProps[m_queueFamilyIndex].queueFlags;

    ResetBarrierState();

    m_enabledMarkers = m_pCmdBuf->VkDevice()->GetRuntimeSettings().devModeSqttMarkerEnable;

    if (SqttMgr::IsTracingSupported(m_pCmdBuf->VkDevice()->VkPhysicalDevice(), m_queueFamilyIndex) == false)
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

    m_cbId = m_pSqttMgr->GetNextCmdBufID(m_pCmdBuf->GetQueueFamilyIndex(), pBeginInfo);

    WriteCbStartMarker();
}

// =====================================================================================================================
// Inserts a CbEnd marker when command buffer building has finished.
void SqttCmdBufferState::End()
{
    WriteCbEndMarker();
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
    const void* pData,
    size_t      dataSize
    ) const
{
    VK_ASSERT(m_enabledMarkers != 0);
    VK_ASSERT((dataSize % sizeof(uint32_t)) == 0);
    VK_ASSERT((dataSize / sizeof(uint32_t)) > 0);

    m_pCmdBuf->PalCmdBuffer()->CmdInsertRgpTraceMarker(static_cast<uint32_t>(dataSize / sizeof(uint32_t)), pData);
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
    RgpSqttMarkerEventType apiType,
    uint32_t               vertexOffsetUserData,
    uint32_t               instanceOffsetUserData,
    uint32_t               drawIndexUserData
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

        WriteMarker(&marker, sizeof(marker));
    }
}

// =====================================================================================================================
// Inserts an RGP pre-dispatch marker
void SqttCmdBufferState::WriteEventWithDimsMarker(
    RgpSqttMarkerEventType apiType,
    uint32_t               x,
    uint32_t               y,
    uint32_t               z
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

        WriteMarker(&eventWithDims, sizeof(eventWithDims));
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

        WriteMarker(m_pUserEvent, markerSize);
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
            drawDispatch.draw.userDataRegs.drawIndex);
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
                drawDispatch.dispatch.groupDims[0],
                drawDispatch.dispatch.groupDims[1],
                drawDispatch.dispatch.groupDims[2]);
        }
        else
        {
            WriteEventMarker(
                m_currentEventType,
                UINT_MAX,
                UINT_MAX,
                UINT_MAX);
        }
    }
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

        // This code that checks the entry point to decipher the barrier reason is temporary code prior to PAL
        // interface version v360 where this value comes straight from the callback field (though it must be provided
        // to PAL from other parts of the driver)
        if (m_currentEntryPoint == RgpSqttMarkerGeneralApiType::CmdPipelineBarrier)
        {
            marker.dword02 = static_cast<uint32_t>(RgpSqttBarrierReason::ExternalCmdPipelineBarrier);
        }
        else if (m_currentEntryPoint == RgpSqttMarkerGeneralApiType::CmdBeginRenderPass ||
                 m_currentEntryPoint == RgpSqttMarkerGeneralApiType::CmdNextSubpass ||
                 m_currentEntryPoint == RgpSqttMarkerGeneralApiType::CmdEndRenderPass)
        {
            marker.dword02 = static_cast<uint32_t>(RgpSqttBarrierReason::ExternalRenderPassSync);
        }
        else if (m_currentEntryPoint == RgpSqttMarkerGeneralApiType::CmdWaitEvents)
        {
            marker.dword02 = static_cast<uint32_t>(RgpSqttBarrierReason::ExternalCmdWaitEvents);
        }
        else
        {
            marker.dword02 = static_cast<uint32_t>(RgpSqttBarrierReason::InternalUnknown);
        }

        WriteMarker(&marker, sizeof(marker));
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

        WriteMarker(&marker, sizeof(marker));
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

        marker.waitOnEopTs          = operations.pipelineStalls.waitOnEopTsBottomOfPipe;
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

        WriteMarker(&marker, sizeof(marker));
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

        WriteMarker(&marker, sizeof(marker));
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

        WriteMarker(&marker, sizeof(marker));
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

        WriteMarker(&marker, sizeof(marker));
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

        WriteMarker(&marker, sizeof(marker));
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
    pSqtt->GetNextLayer()->entry_name

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
    SQTT_SETUP();

    pSqtt->BeginEntryPoint(RgpSqttMarkerGeneralApiType::CmdBindPipeline);

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
            if (GraphicsPipeline::IsNullHandle(pPipelines[i]) == false)
            {
                GraphicsPipeline* pPipeline = NonDispatchable<VkPipeline, GraphicsPipeline>::ObjectFromHandle(
                    pPipelines[i]);

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
            if (ComputePipeline::IsNullHandle(pPipelines[i]) == false)
            {
                ComputePipeline* pPipeline = NonDispatchable<VkPipeline, ComputePipeline>::ObjectFromHandle(
                    pPipelines[i]);

#if ICD_GPUOPEN_DEVMODE_BUILD
                pDevMgr->PipelineCreated(pDevice, pPipeline);
#endif
            }
        }
    }

    return result;
}

#define SQTT_DISPATCH_ENTRY(entry_name) VK_DISPATCH_ENTRY(entry_name, vk::entry::sqtt::entry_name)

// This is the SQTT layer dispatch table.  It contains an entry for every Vulkan entry point that this layer shadows.
const DispatchTableEntry g_SqttDispatchTable[] =
{
    // Command buffer functions
    SQTT_DISPATCH_ENTRY(vkCmdBindPipeline),
    SQTT_DISPATCH_ENTRY(vkCmdBindDescriptorSets),
    SQTT_DISPATCH_ENTRY(vkCmdBindIndexBuffer),
    SQTT_DISPATCH_ENTRY(vkCmdBindVertexBuffers),
    SQTT_DISPATCH_ENTRY(vkCmdDraw),
    SQTT_DISPATCH_ENTRY(vkCmdDrawIndexed),
    SQTT_DISPATCH_ENTRY(vkCmdDrawIndirect),
    SQTT_DISPATCH_ENTRY(vkCmdDrawIndexedIndirect),
    SQTT_DISPATCH_ENTRY(vkCmdDrawIndirectCountAMD),
    SQTT_DISPATCH_ENTRY(vkCmdDrawIndexedIndirectCountAMD),
    SQTT_DISPATCH_ENTRY(vkCmdDispatch),
    SQTT_DISPATCH_ENTRY(vkCmdDispatchIndirect),
    SQTT_DISPATCH_ENTRY(vkCmdCopyBuffer),
    SQTT_DISPATCH_ENTRY(vkCmdCopyImage),
    SQTT_DISPATCH_ENTRY(vkCmdBlitImage),
    SQTT_DISPATCH_ENTRY(vkCmdCopyBufferToImage),
    SQTT_DISPATCH_ENTRY(vkCmdCopyImageToBuffer),
    SQTT_DISPATCH_ENTRY(vkCmdUpdateBuffer),
    SQTT_DISPATCH_ENTRY(vkCmdFillBuffer),
    SQTT_DISPATCH_ENTRY(vkCmdClearColorImage),
    SQTT_DISPATCH_ENTRY(vkCmdClearDepthStencilImage),
    SQTT_DISPATCH_ENTRY(vkCmdClearAttachments),
    SQTT_DISPATCH_ENTRY(vkCmdResolveImage),
    SQTT_DISPATCH_ENTRY(vkCmdWaitEvents),
    SQTT_DISPATCH_ENTRY(vkCmdPipelineBarrier),
    SQTT_DISPATCH_ENTRY(vkCmdBeginQuery),
    SQTT_DISPATCH_ENTRY(vkCmdEndQuery),
    SQTT_DISPATCH_ENTRY(vkCmdResetQueryPool),
    SQTT_DISPATCH_ENTRY(vkCmdWriteTimestamp),
    SQTT_DISPATCH_ENTRY(vkCmdCopyQueryPoolResults),
    SQTT_DISPATCH_ENTRY(vkCmdPushConstants),
    SQTT_DISPATCH_ENTRY(vkCmdBeginRenderPass),
    SQTT_DISPATCH_ENTRY(vkCmdNextSubpass),
    SQTT_DISPATCH_ENTRY(vkCmdEndRenderPass),
    SQTT_DISPATCH_ENTRY(vkCmdExecuteCommands),
    SQTT_DISPATCH_ENTRY(vkCmdSetViewport),
    SQTT_DISPATCH_ENTRY(vkCmdSetScissor),
    SQTT_DISPATCH_ENTRY(vkCmdSetLineWidth),
    SQTT_DISPATCH_ENTRY(vkCmdSetDepthBias),
    SQTT_DISPATCH_ENTRY(vkCmdSetBlendConstants),
    SQTT_DISPATCH_ENTRY(vkCmdSetDepthBounds),
    SQTT_DISPATCH_ENTRY(vkCmdSetStencilCompareMask),
    SQTT_DISPATCH_ENTRY(vkCmdSetStencilWriteMask),
    SQTT_DISPATCH_ENTRY(vkCmdSetStencilReference),
    SQTT_DISPATCH_ENTRY(vkCmdDebugMarkerBeginEXT),
    SQTT_DISPATCH_ENTRY(vkCmdDebugMarkerEndEXT),
    SQTT_DISPATCH_ENTRY(vkCmdDebugMarkerInsertEXT),

    // Device functions
    SQTT_DISPATCH_ENTRY(vkCreateGraphicsPipelines),
    SQTT_DISPATCH_ENTRY(vkCreateComputePipelines),

    VK_DISPATCH_TABLE_END()
};

} // namespace sqtt

} // namespace entry

} // namespace vk
