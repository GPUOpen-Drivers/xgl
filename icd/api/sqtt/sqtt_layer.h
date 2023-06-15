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
 * @file  sqtt_layer.h
 * @brief Contains shadowed entry points related to SQTT marker annotation support.  It is primarily used by the
 *        developer mode driver in conjunction with the RGP tool.
 ***********************************************************************************************************************
 */

#ifndef __SQTT_SQTT_LAYER_H__
#define __SQTT_SQTT_LAYER_H__

#pragma once

#include <limits.h>

#include "include/khronos/vulkan.h"

#include "include/vk_dispatch.h"
#include "include/vk_queue.h"

#include "sqtt/sqtt_rgp_annotations.h"

#include "palList.h"
#include "palHashMap.h"

namespace vk
{

class CmdBuffer;
class Device;
class ImageView;
class RenderPass;
class SqttMgr;
class Pipeline;
class DevModeMgr;

// Contains parameters that are happening when renderpass targets are bound in the driver.
struct SqttBindTargetParams
{
    const ImageView*             pColorTargets[Pal::MaxColorTargets];
    const ImageView*             pDepthStencil;
    const Pal::BindTargetParams* pBindParams;
};

// =====================================================================================================================
// This is an auxiliary structure that tracks whatever queue-level state is necessary to handle SQTT marker
// annotations.
class SqttQueueState
{
public:
    SqttQueueState(Queue* pQueue);
    ~SqttQueueState();

    VkResult Init();

    void DebugLabelBegin(const VkDebugUtilsLabelEXT* pMarkerInfo);
    void DebugLabelEnd();
    void DebugLabelInsert(const VkDebugUtilsLabelEXT* pMarkerInfo);

    void WritePresentMarker(Pal::ICmdBuffer* pCmdBuffer, bool* pAddedMarker) const;

    const DispatchTable* GetNextLayer() const
        { return m_pNextLayer; }

private:

    void SubmitUserEventMarker(RgpSqttMarkerUserEventType eventType, const char* pString);

    union CmdBufferMapKey
    {
        struct
        {
            uint32_t stringHash : 32;
            uint32_t eventType  : 3;
            uint32_t reserved   : 29;
        };
        uint64_t u64All;
    };

    typedef Util::HashMap<uint64_t, VkCommandBuffer, PalAllocator> CmdBufferMap;

    VkCommandPool        m_cmdPool;
    Util::RWLock         m_lock;
    Queue*               m_pQueue;
    Device*              m_pDevice;
    CmdBufferMap         m_cmdBufferMap; // Local command buffer cache
    const DispatchTable* m_pNextLayer;   // Pointer to next layer's dispatch table
    uint32_t             m_enabledMarkers; // RGP Markers
};

// =====================================================================================================================
// This is an auxiliary structure that tracks whatever cmdbuffer-level state is necessary to handle SQTT marker
// annotations.
class SqttCmdBufferState
{
public:
    SqttCmdBufferState(CmdBuffer* pCmdBuf);
    ~SqttCmdBufferState();

    void Begin(const VkCommandBufferBeginInfo* pBeginInfo);
    void End();

    void BeginEntryPoint(RgpSqttMarkerGeneralApiType apiType = RgpSqttMarkerGeneralApiType::Invalid);
    void EndEntryPoint();

    void BeginEventMarkers(RgpSqttMarkerEventType apiType);
    void EndEventMarkers();

    void BeginRenderPassColorClear();
    void EndRenderPassColorClear();

    void BeginRenderPassDepthStencilClear();
    void EndRenderPassDepthStencilClear();

    void BeginRenderPassResolve();
    void EndRenderPassResolve();

    void PipelineBound(VkPipelineBindPoint bindPoint, VkPipeline pipeline);

    const DispatchTable* GetNextLayer() const
        { return m_pNextLayer; }

    CmdBuffer* GetParent() const
        { return m_pCmdBuf; }

    RgpSqttMarkerCbID GetId() const
        { return m_cbId; }

    void PalBarrierCallback(
        Pal::Developer::CallbackType       type,
        const Pal::Developer::BarrierData& barrier);

    void PalDrawDispatchCallback(
        const Pal::Developer::DrawDispatchData& drawDispatch);

    void PalBindPipelineCallback(
        const Pal::Developer::BindPipelineData& bindPipeline);

    void DebugMarkerBegin(const VkDebugMarkerMarkerInfoEXT* pMarkerInfo);
    void DebugMarkerEnd();
    void DebugMarkerInsert(const VkDebugMarkerMarkerInfoEXT* pMarkerInfo);

    void DebugLabelBegin(const VkDebugUtilsLabelEXT* pMarkerInfo);
    void DebugLabelEnd();
    void DebugLabelInsert(const VkDebugUtilsLabelEXT* pMarkerInfo);

    void WriteUserEventMarker(RgpSqttMarkerUserEventType eventType, const char* pString) const;

    void AddDebugTag(uint64_t tag);
    bool HasDebugTag(uint64_t tag) const;

private:
    RgpSqttMarkerEvent BuildEventMarker(RgpSqttMarkerEventType apiType);
    void WriteCbStartMarker() const;
    void WriteCbEndMarker() const;
    void WritePipelineBindMarker(const Pal::Developer::BindPipelineData& data) const;
    void WriteMarker(const void* pData, size_t dataSize, Pal::RgpMarkerSubQueueFlags subQueueFlags) const;
    void WriteBeginGeneralApiMarker(RgpSqttMarkerGeneralApiType apiType) const;
    void WriteEndGeneralApiMarker(RgpSqttMarkerGeneralApiType apiType) const;
    void WriteBarrierStartMarker(const Pal::Developer::BarrierData& data) const;
    void WriteLayoutTransitionMarker(const Pal::Developer::BarrierData& data) const;
    void WriteBarrierEndMarker(const Pal::Developer::BarrierData& data) const;
    void ResetBarrierState();
    void WriteEventMarker(
        RgpSqttMarkerEventType      apiType,
        uint32_t                    vertexOffsetUserData,
        uint32_t                    instanceOffsetUserData,
        uint32_t                    drawIndexUserData,
        Pal::RgpMarkerSubQueueFlags subQueueFlags);
    void WriteEventWithDimsMarker(
        RgpSqttMarkerEventType      apiType,
        uint32_t                    x,
        uint32_t                    y,
        uint32_t                    z,
        Pal::RgpMarkerSubQueueFlags subQueueFlags);

    CmdBuffer*                  m_pCmdBuf;
    SqttMgr*                    m_pSqttMgr;          // Per-device SQTT state
    DevModeMgr*                 m_pDevModeMgr;
    const RuntimeSettings&      m_settings;
    const DispatchTable*        m_pNextLayer;        // Pointer to next layer's dispatch table
    RgpSqttMarkerCbID           m_cbId;              // Command buffer ID associated with this command buffer
    uint64_t                    m_deviceId;          // API-specific device ID (we use VkDevice handle)
    uint32_t                    m_queueFamilyIndex;
    VkQueueFlags                m_queueFamilyFlags;
    RgpSqttMarkerGeneralApiType m_currentEntryPoint; // Current general API identifier for the entry point we're in
    uint32_t                    m_currentEventId;    // Current command ID for pre-draw/dispatch event markers
    RgpSqttMarkerEventType      m_currentEventType;  // Current API type for pre-draw/dispatch event markers
    uint32_t                    m_enabledMarkers;

#if ICD_GPUOPEN_DEVMODE_BUILD
    struct
    {
        bool                started;    // True if a pipeline is currently being traced
        uint64_t            targetHash; // Determines target pipeline used to trigger instruction tracing
        VkPipelineBindPoint bindPoint;  // Bind point of the target pipeline
    } m_instructionTrace;
#endif

    RgpSqttMarkerUserEventWithString* m_pUserEvent;

    // Some persistent state for tracking barrier information
    struct
    {
        uint16_t pipelineStalls;       // The pipeline stalls seen since BarrierStart
        uint16_t caches;               // The cache flushes/invals seen since BarrierStart
        uint32_t numLayoutTransitions; // Number of layout transitions since the last BarrierStart
        bool     inside;               // True if inside a barrier begin/end
    } m_currentBarrier;

    Util::List<uint64_t, PalAllocator> m_debugTags;
};

void SqttOverrideDispatchTable(DispatchTable* pDispatchTable, SqttMgr* pMgr);

}; // namespace vk

#endif /* __SQTT_SQTT_LAYER_H__ */
