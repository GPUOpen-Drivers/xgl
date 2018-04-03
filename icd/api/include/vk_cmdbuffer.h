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
 * @file  vk_cmdbuffer.h
 * @brief Implementation of Vulkan command buffer class.
 **************************************************************************************************
 */

#ifndef __VK_CMDBUFFER_H__
#define __VK_CMDBUFFER_H__

#pragma once

#include "include/khronos/vulkan.h"

#include "include/vk_cmd_pool.h"
#include "include/vk_event.h"
#include "include/vk_dispatch.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_utils.h"

#include "include/gpu_event_mgr.h"
#include "include/internal_mem_mgr.h"
#include "include/stencil_ops_combiner.h"
#include "include/vert_buf_binding_mgr.h"
#include "include/virtual_stack_mgr.h"

#include "renderpass/renderpass_builder.h"

#include "palCmdBuffer.h"
#include "palDequeImpl.h"
#include "palGpuMemory.h"
#include "palLinearAllocator.h"
#include "palPipeline.h"
#include "palQueue.h"

// Forward declare PAL classes used in this file
namespace Pal
{

struct BarrierInfo;
struct CmdBufferCreateInfo;
class  ICmdBuffer;
class  IGpuEvent;
struct ImageLayout;

};

namespace vk
{

// Forward declare Vulkan classes used in this file
class ComputePipeline;
class Device;
class DispatchableCmdBuffer;
class Framebuffer;
class GraphicsPipeline;
class Image;
class Queue;
class RenderPass;
class TimestampQueryPool;
class SqttCmdBufferState;

// =====================================================================================================================
// Represents an internal GPU allocation owned by a Vulkan command buffer.  Can contain things like internal descriptor
// set data and other non-PM4 related data.
struct CmdBufGpuMem
{
    InternalMemory           internalMem;  // Internal memory allocation
    InternalMemCreateInfo    info;         // Information about this allocation
    CmdBufGpuMem*            pNext;        // Intrusive list pointer to the next command buffer GPU memory object.
};

constexpr uint32_t MaxDescSetRegCount   = MaxDescriptorSets * PipelineLayout::SetPtrRegCount;
constexpr uint32_t MaxDynDescRegCount   = MaxDynamicDescriptors * PipelineLayout::DynDescRegCount;
constexpr uint32_t MaxBindingRegCount   = MaxDescSetRegCount + MaxDynDescRegCount;
constexpr uint32_t MaxPushConstRegCount = MaxPushConstants / 4;

// This structure contains information about currently written user data entries within the command buffer
struct PipelineBindState
{
    // Cached copy of the user data layout from the current pipeline's layout
    PipelineLayout::UserDataLayout userDataLayout;
    // Current pipeline's layout
    const PipelineLayout* pLayout;
    // High-water mark of the largest number of bound sets
    uint32_t boundSetCount;
    // High-water mark of the largest number of pushed constants
    uint32_t pushedConstCount;
    // Currently pushed constant values (relative to an base = 0)
    uint32_t pushConstData[MaxPushConstRegCount];
};

// Members of CmdBufferRenderState that are different for each GPU
struct PerGpuRenderState
{
    // Any members added to this structure may need to be cleared in CmdBuffer::ResetState().
    const Pal::IMsaaState*          pMsaaState;
    const Pal::IColorBlendState*    pColorBlendState;
    const Pal::IDepthStencilState*  pDepthStencilState;
    // Currently bound descriptor sets and dynamic offsets (relative to base = 00)
    uint32_t setBindingData[static_cast<uint32_t>(Pal::PipelineBindPoint::Count)][MaxBindingRegCount];
};

// Members of CmdBufferRenderState that are the same for each GPU
struct AllGpuRenderState
{
    const GraphicsPipeline*        pGraphicsPipeline;
    const ComputePipeline*         pComputePipeline;
    const RenderPass*              pRenderPass;
    const Framebuffer*             pFramebuffer;
    const Pal::IMsaaState* const * pBltMsaaStates;

    // These tokens describe the current "static" values of pieces of Vulkan render state.  These are set by pipelines
    // that program static render state, and are reset to DynamicRenderStateToken by vkCmdSet* functions.
    //
    // Command buffer recording can compare these tokens with new incoming tokens to efficiently redundancy check
    // render state and avoid context rolling.  This redundancy checking is only done for static pipeline state and not
    // for vkCmdSet* function values.
    struct
    {
        uint32_t inputAssemblyState;
        uint32_t triangleRasterState;
        uint32_t pointLineRasterState;
        uint32_t depthBiasState;
        uint32_t blendConst;
        uint32_t depthBounds;
        uint32_t viewports;
        uint32_t scissorRect;
        uint32_t samplePattern;
    } staticTokens;

    // Value of VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
    // defined by the last bound GraphicsPipeline, which was not nullptr.
    bool ViewIndexFromDeviceIndex;

// =====================================================================================================================
// The first part of the structure will be cleared with a memset in CmdBuffer::ResetState().
// The second part of the structure contains the larger members that are selectively reset in CmdBuffer::ResetState().
// =====================================================================================================================
    // Keep pipelineState as the first member of the section that is selectively reset.  It is used to compute how large
    // the first part is for the memset in CmdBuffer::ResetState().
    PipelineBindState       pipelineState[static_cast<uint32_t>(Pal::PipelineBindPoint::Count)];
    Pal::ScissorRectParams  scissor;
    Pal::ViewportParams     viewport;
};

// This structure describes current render state within a command buffer during its building.
struct CmdBufferRenderState
{
    AllGpuRenderState allGpuState;

    PerGpuRenderState perGpuState[MaxPalDevices];
};

// State tracked during a render pass instance when building a command buffer.
struct RenderPassInstanceState
{
    // Per-attachment instance state
    struct AttachmentState
    {
        Pal::ImageLayout aspectLayout[3];       // Current per-aspect (color, depth, stencil) PAL layout
        VkClearValue     clearValue;            // Specified load-op clear value for this attachment
        SamplePattern    initialSamplePattern;  // Initial sample pattern at first layout transition of
                                                // depth/stencil attachment.
    };

    union
    {
        struct
        {
            uint32_t samplePatternValid : 1;
            uint32_t reserved : 31;
        };
        uint32_t u32All;
    } flags;

    RenderPassInstanceState(PalAllocator* pAllocator);

    const RenderPassExecuteInfo*                pExecuteInfo;
    uint32_t                                    subpass;
    uint32_t                                    renderAreaCount;
    Pal::Rect                                   renderArea[MaxPalDevices];
    size_t                                      maxAttachmentCount;
    AttachmentState*                            pAttachments;
    size_t                                      maxSubpassCount;
    SamplePattern*                              pSamplePatterns;
};

// =====================================================================================================================
// A Vulkan command buffer.
class CmdBuffer
{
public:
    typedef VkCommandBuffer ApiType;

    static VkResult Create(
        Device*                            pDevice,
        const VkCommandBufferAllocateInfo* pAllocateInfo,
        VkCommandBuffer*                   pCommandBuffers);

    VkResult Begin(
        const VkCommandBufferBeginInfo*             pBeginInfo);

    VkResult Reset(VkCommandBufferResetFlags flags);

    VkResult End(void);

    void BindPipeline(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipeline                                  pipeline);
    void ExecuteCommands(
        uint32_t                                    cmdBufferCount,
        const VkCommandBuffer*                      pCmdBuffers);

    void BindDescriptorSets(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        const VkDescriptorSet*                      pDescriptorSets,
        uint32_t                                    dynamicOffsetCount,
        const uint32_t*                             pDynamicOffsets);

    void BindIndexBuffer(
        VkBuffer                                    buffer,
        VkDeviceSize                                offset,
        VkIndexType                                 indexType);

    void BindVertexBuffers(
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        const VkBuffer*                             pBuffers,
        const VkDeviceSize*                         pOffsets);

    void Draw(
        uint32_t                                    firstVertex,
        uint32_t                                    vertexCount,
        uint32_t                                    firstInstance,
        uint32_t                                    instanceCount);

    void DrawIndexed(
        uint32_t                                    firstIndex,
        uint32_t                                    indexCount,
        int32_t                                     vertexOffset,
        uint32_t                                    firstInstance,
        uint32_t                                    instanceCount);

    template< bool indexed, bool useBufferCount>
    void DrawIndirect(
        VkBuffer                                    buffer,
        VkDeviceSize                                offset,
        uint32_t                                    count,
        uint32_t                                    stride,
        VkBuffer                                    countBuffer,
        VkDeviceSize                                countOffset);

    void Dispatch(
        uint32_t                                    x,
        uint32_t                                    y,
        uint32_t                                    z);

    void DispatchOffset(
        uint32_t                                    base_x,
        uint32_t                                    base_y,
        uint32_t                                    base_z,
        uint32_t                                    dim_x,
        uint32_t                                    dim_y,
        uint32_t                                    dim_z);

    void DispatchIndirect(
        VkBuffer                                    buffer,
        VkDeviceSize                                offset);

    void CopyBuffer(
        VkBuffer                                    srcBuffer,
        VkBuffer                                    destBuffer,
        uint32_t                                    regionCount,
        const VkBufferCopy*                         pRegions);

    void CopyImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const VkImageCopy*                          pRegions);

    void BlitImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const VkImageBlit*                          pRegions,
        VkFilter                                    filter);

    void CopyBufferToImage(
        VkBuffer                                    srcBuffer,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const VkBufferImageCopy*                    pRegions);

    void CopyImageToBuffer(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkBuffer                                    destBuffer,
        uint32_t                                    regionCount,
        const VkBufferImageCopy*                    pRegions);

    void UpdateBuffer(
        VkBuffer                                    destBuffer,
        VkDeviceSize                                destOffset,
        VkDeviceSize                                dataSize,
        const uint32_t*                             pData);

    void FillBuffer(
        VkBuffer                                    destBuffer,
        VkDeviceSize                                destOffset,
        VkDeviceSize                                fillSize,
        uint32_t                                    data);

    void ClearColorImage(
        VkImage                                     image,
        VkImageLayout                               imageLayout,
        const VkClearColorValue*                    pColor,
        uint32_t                                    rangeCount,
        const VkImageSubresourceRange*              pRanges);

    void ClearDepthStencilImage(
        VkImage                                     image,
        VkImageLayout                               imageLayout,
        float                                       depth,
        uint32_t                                    stencil,
        uint32_t                                    rangeCount,
        const VkImageSubresourceRange*              pRanges);

    void ClearAttachments(
        uint32_t                                    attachmentCount,
        const VkClearAttachment*                    pAttachments,
        uint32_t                                    rectCount,
        const VkClearRect*                          pRects);

    void ClearImageAttachments(
        uint32_t                                    attachmentCount,
        const VkClearAttachment*                    pAttachments,
        uint32_t                                    rectCount,
        const VkClearRect*                          pRects);

    void ClearBoundAttachments(
        uint32_t                                    attachmentCount,
        const VkClearAttachment*                    pAttachments,
        uint32_t                                    rectCount,
        const VkClearRect*                          pRects);

    void ResolveImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    rectCount,
        const VkImageResolve*                       pRects);

    void SetViewport(
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        const VkViewport*                           pViewports);

    void SetAllViewports(
        const Pal::ViewportParams&                  params,
        uint32_t                                    staticToken);

    void SetScissor(
        uint32_t                                    firstScissor,
        uint32_t                                    scissorCount,
        const VkRect2D*                             pScissors);

    void SetAllScissors(
        const Pal::ScissorRectParams&               params,
        uint32_t                                    staticToken);

    void SetLineWidth(
        float                                       lineWidth);

    void SetDepthBias(
        float                                       depthBias,
        float                                       depthBiasClamp,
        float                                       slopeScaledDepthBias);

    void SetBlendConstants(
        const float                                 blendConst[4]);

    void SetDepthBounds(
        float                                       minDepthBounds,
        float                                       maxDepthBounds);

    void SetViewInstanceMask();

    void SetStencilCompareMask(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilCompareMask);

    void SetStencilWriteMask(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilWriteMask);

    void SetStencilReference(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilReference);

    void SetEvent(
        VkEvent                                     event,
        VkPipelineStageFlags                        stageMask);

    void ResetEvent(
        VkEvent                                     event,
        VkPipelineStageFlags                        stageMask);

    void WaitEvents(
        uint32_t                                    eventCount,
        const VkEvent*                              pEvents,
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers);

    void PipelineBarrier(
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers);

    void BeginQuery(
        VkQueryPool                                 queryPool,
        uint32_t                                    query,
        VkQueryControlFlags                         flags);

    void EndQuery(
        VkQueryPool                                 queryPool,
        uint32_t                                    query);

    void ResetQueryPool(
        VkQueryPool                                 queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount);

    void CopyQueryPoolResults(
        VkQueryPool                                 queryPool,
        uint32_t                                    firstQuery,
        uint32_t                                    queryCount,
        VkBuffer                                    destBuffer,
        VkDeviceSize                                destOffset,
        VkDeviceSize                                destStride,
        VkQueryResultFlags                          flags);

    void WriteTimestamp(
        VkPipelineStageFlagBits   pipelineStage,
        const TimestampQueryPool* pQueryPool,
        uint32_t                  query);

    void SetSampleLocations(
        const VkSampleLocationsInfoEXT* pSampleLocationsInfo);

    void BeginRenderPass(
        const VkRenderPassBeginInfo* pRenderPassBegin,
        VkSubpassContents            contents);

    void NextSubPass(VkSubpassContents contents);
    void EndRenderPass();

    void PushConstants(
        VkPipelineLayout                            layout,
        VkShaderStageFlags                          stageFlags,
        uint32_t                                    start,
        uint32_t                                    length,
        const void*                                 values);

    void WriteBufferMarker(
        VkPipelineStageFlagBits pipelineStage,
        VkBuffer                dstBuffer,
        VkDeviceSize            dstOffset,
        uint32_t                marker);

    VK_INLINE void SetDeviceMask(uint32_t deviceMask)
    {
        // Ensure we are enabling valid devices within the group
        VK_ASSERT((m_pDevice->GetPalDeviceMask() & deviceMask) == deviceMask);

        // Ensure disabled devices are not enabled during recording
        VK_ASSERT(((m_palDeviceUsedMask ^ deviceMask) & deviceMask) == 0);

        m_palDeviceMask = deviceMask;
    }

    VK_INLINE uint32_t GetDeviceMask() const
    {
        return m_palDeviceMask;
    }

    VK_INLINE uint32_t GetDeviceUsedMask() const
    {
        return m_palDeviceUsedMask;
    }

    VkResult Destroy(void);

    VK_FORCEINLINE Device* VkDevice(void) const
        { return m_pDevice; }

    VK_FORCEINLINE Instance* VkInstance(void) const
        { return m_pDevice->VkInstance(); }

    VK_INLINE Pal::ICmdBuffer* PalCmdBuffer(
            int32_t idx = DefaultDeviceIndex) const
    {
        if (idx == 0)
        {
            VK_ASSERT((uintptr_t)m_pPalCmdBuffers[idx] == (uintptr_t)this + sizeof(*this));
            return (Pal::ICmdBuffer*)((uintptr_t)this + sizeof(*this));
        }

        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalCmdBuffers[idx];
    }

    static Pal::uint32 ConvertBarrierSrcAccessFlags(const Device* pDevice, VkAccessFlags accessMask);
    static Pal::uint32 ConvertBarrierDstAccessFlags(
               const Device* pDevice,
               VkAccessFlags accessMask,
               uint32_t      combinedAccessMask);
    static void ConvertBarrierCacheFlags(
               const Device*           pDevice,
               VkAccessFlags           srcAccess,
               VkAccessFlags           dstAccess,
               uint32_t                supportInputCacheMask,
               uint32_t                supportOutputCacheMask,
               uint32_t                barrierOptions,
               Pal::BarrierTransition* pResult);

    VK_INLINE uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }
    VK_INLINE Pal::QueueType GetPalQueueType() const { return m_palQueueType; }
    VK_INLINE Pal::EngineType GetPalEngineType() const { return m_palEngineType; }

    VK_INLINE VirtualStackAllocator* GetStackAllocator() { return m_pStackAllocator; }

    void RequestRenderPassEvents(uint32_t eventCount, GpuEvents*** pppGpuEvents);

    void PalCmdBarrier(
        const Pal::BarrierInfo& info);

    void PalCmdBarrier(
        Pal::BarrierInfo*             pInfo,
        Pal::BarrierTransition* const pTransitions,
        const Image** const           pTransitionImages);

    Pal::Result PalCmdBufferBegin(
            const Pal::CmdBufferBuildInfo& cmdInfo);

    Pal::Result PalCmdBufferEnd();
    Pal::Result PalCmdBufferReset(Pal::ICmdAllocator* pCmdAllocator, bool returnGpuMemory);

    void PalCmdBufferDestroy();

    void PalCmdBindIndexData(
                Buffer* pBuffer,
                Pal::gpusize offset,
                Pal::IndexType indexType);

    void PalCmdUnbindIndexData(Pal::IndexType indexType);

    void PalCmdDraw(
        uint32_t firstVertex,
        uint32_t vertexCount,
        uint32_t firstInstance,
        uint32_t instanceCount);

    void PalCmdDrawIndexed(
        uint32_t firstIndex,
        uint32_t indexCount,
        int32_t  vertexOffset,
        uint32_t firstInstance,
        uint32_t instanceCount);

    void PalCmdDispatch(
        uint32_t x,
        uint32_t y,
        uint32_t z);

    void PalCmdDispatchOffset(
        uint32_t base_x,
        uint32_t base_y,
        uint32_t base_z,
        uint32_t size_x,
        uint32_t size_y,
        uint32_t size_z);

    void PalCmdDispatchIndirect(
        Buffer*      pBuffer,
        Pal::gpusize offset);

    void PalCmdCopyBuffer(
        Buffer*                pSrcBuffer,
        Buffer*                pDstBuffer,
        uint32_t               regionCount,
        Pal::MemoryCopyRegion* pRegions);

    void PalCmdCopyImage(
        const Image* const    pSrcImage,
        Pal::ImageLayout      srcImageLayout,
        const Image* const    pDstImage,
        Pal::ImageLayout      destImageLayout,
        uint32_t              regionCount,
        Pal::ImageCopyRegion* pRegions);

    void PalCmdScaledCopyImage(
        const Image* const   pSrcImage,
        const Image* const   pDstImage,
        Pal::ScaledCopyInfo& copyInfo);

    void PalCmdCopyMemoryToImage(
        const Buffer*               pSrcBuffer,
        const Image*                pDstImage,
        Pal::ImageLayout            layout,
        uint32_t                    regionCount,
        Pal::MemoryImageCopyRegion* pRegions);

    void PalCmdCopyImageToMemory(
        const Image*                pSrcImage,
        const Buffer*               pDstBuffer,
        Pal::ImageLayout            layout,
        uint32_t                    regionCount,
        Pal::MemoryImageCopyRegion* pRegions);

    void PalCmdUpdateBuffer(
        Buffer*         pDestBuffer,
        Pal::gpusize    offset,
        Pal::gpusize    size,
        const uint32_t* pData);

    void PalCmdFillBuffer(
        Buffer*         pDestBuffer,
        Pal::gpusize    offset,
        Pal::gpusize    size,
        uint32_t        data);

    void PalCmdClearColorImage(
        const Image&            image,
        Pal::ImageLayout        imageLayout,
        const Pal::ClearColor&  color,
        Pal::uint32             rangeCount,
        const Pal::SubresRange* pRanges,
        Pal::uint32             boxCount,
        const Pal::Box*         pBoxes,
        Pal::uint32             flags);

    void PalCmdClearDepthStencil(
        const Image&            image,
        Pal::ImageLayout        depthLayout,
        Pal::ImageLayout        stencilLayout,
        float                   depth,
        Pal::uint8              stencil,
        Pal::uint32             rangeCount,
        const Pal::SubresRange* pRanges,
        Pal::uint32             rectCount,
        const Pal::Rect*        pRects,
        Pal::uint32             flags);

    template <typename EventContainer_T>
    void PalCmdResetEvent(
        EventContainer_T*    pEvent,
        Pal::HwPipePoint     resetPoint);

    template <typename EventContainer_T>
    void PalCmdSetEvent(
        EventContainer_T*    pEvent,
        Pal::HwPipePoint     resetPoint);

    template< bool regionPerDevice >
    void PalCmdResolveImage(
        const Image&                   srcImage,
        Pal::ImageLayout               srcImageLayout,
        const Image&                   dstImage,
        Pal::ImageLayout               dstImageLayout,
        uint32_t                       regionCount,
        const Pal::ImageResolveRegion* pRegions);

    void PalCmdSetIndirectUserDataWatermark(
        uint16_t      tableId,
        uint32_t      dwordLimit);

    void PreBltBindMsaaState(const Image& image);

    void PostBltRestoreMsaaState();

    void PalCmdBindMsaaStates(const Pal::IMsaaState* const * pStates);

    VK_INLINE void PalCmdBindMsaaState(
        Pal::ICmdBuffer*       pPalCmdBuf,
        uint32_t               deviceIdx,
        const Pal::IMsaaState* pState);

    VK_INLINE void PalCmdBindColorBlendState(
        Pal::ICmdBuffer*             pPalCmdBuf,
        uint32_t                     deviceIdx,
        const Pal::IColorBlendState* pState);

    VK_INLINE void PalCmdBindDepthStencilState(
        Pal::ICmdBuffer*               pPalCmdBuf,
        uint32_t                       deviceIdx,
        const Pal::IDepthStencilState* pState);

    void PalCmdSetMsaaQuadSamplePattern(
        uint32_t numSamplesPerPixel,
        const  Pal::MsaaQuadSamplePattern& quadSamplePattern);

    VK_INLINE void PalCmdBufferSetUserData(
        Pal::PipelineBindPoint bindPoint,
        uint32_t               firstEntry,
        uint32_t               entryCount,
        uint32_t               perDeviceStride,
        const uint32_t*        pEntryValues);

    template< typename EventContainer_T >
    VK_INLINE void InsertDeviceEvents(
        const Pal::IGpuEvent**  pDestEvents,
        const EventContainer_T* pSrcEvents,
        uint32_t                index,
        uint32_t                stride) const;

    VK_INLINE uint32_t NumDeviceEvents(uint32_t numEvents) const
    {
        return m_pDevice->NumPalDevices() * numEvents;
    }

#if VK_ENABLE_DEBUG_BARRIERS
    VK_INLINE void DbgBarrierPreCmd(uint32_t cmd)
    {
        if (m_dbgBarrierPreCmdMask & (cmd))
        {
            DbgCmdBarrier(true);
        }
    }
    VK_INLINE void DbgBarrierPostCmd(uint32_t cmd)
    {
        if (m_dbgBarrierPostCmdMask & (cmd))
        {
            DbgCmdBarrier(false);
        }
    }
#else
    VK_INLINE void DbgBarrierPreCmd(uint32_t cmd) {}
    VK_INLINE void DbgBarrierPostCmd(uint32_t cmd) {}
#endif

    SqttCmdBufferState* GetSqttState()
        { return m_pSqttState; }

    VK_INLINE static bool IsStaticStateDifferent(
        uint32_t oldToken,
        uint32_t newToken);

private:
    CmdBuffer(Device* pDevice, CmdPool* pCmdPool, uint32_t queueFamilyIndex);

    VkResult Initialize(
        void*                           pPalMem,
        void*                           pVbMem,
        const Pal::CmdBufferCreateInfo& createInfo);

    void ResetState();

    void FlushBarriers(
        Pal::BarrierInfo*              pBarrier,
        Pal::BarrierTransition* const  pTransitions,
        const Image**                  pTransitionImages,
        uint32_t                       mainTransitionCount,
        uint32_t                       postTransitionStartIdx,
        uint32_t                       postTransitionCount);

    void ExecuteBarriers(
        VirtualStackFrame&           virtStackFrame,
        uint32_t                     memBarrierCount,
        const VkMemoryBarrier*       pMemoryBarriers,
        uint32_t                     bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers,
        uint32_t                     imageMemoryBarrierCount,
        const VkImageMemoryBarrier*  pImageMemoryBarriers,
        Pal::BarrierInfo*            pBarrier);

    void RebindCompatibleUserData(
        uint32_t               bindPoint,
        const PipelineLayout*  pNewLayout);

    void PalBindPipeline(
        VkPipelineBindPoint     pipelineBindPoint,
        VkPipeline              pipeline);

    void AlignMemoryImageCopyRegion(
        const Pal::IImage*          pImage,
        Pal::MemoryImageCopyRegion* pRegion) const;

    template< typename Type_T >
    bool DetectCopyOverwrite(const Type_T* pDst) const;

    VK_INLINE void RPBeginSubpass();
    VK_INLINE void RPEndSubpass();
    void RPResolveAttachments(uint32_t count, const RPResolveInfo* pResolves);
    void RPSyncPoint(const RPSyncPointInfo& syncPoint, VirtualStackFrame* pVirtStack);
    void RPLoadOpClearColor(uint32_t count, const RPLoadOpClearInfo* pClears);
    void RPLoadOpClearDepthStencil(uint32_t count, const RPLoadOpClearInfo* pClears);
    void RPBindTargets(const RPBindTargetsInfo& targets);
    void RPInitSamplePattern();

    VK_INLINE Pal::ImageLayout RPGetAttachmentLayout(uint32_t attachment, Pal::ImageAspect aspect);
    VK_INLINE void RPSetAttachmentLayout(uint32_t attachment, Pal::ImageAspect aspect, Pal::ImageLayout layout);

    void FillTimestampQueryPool(
        const TimestampQueryPool& timestampQueryPool,
        const uint32_t            firstQuery,
        const uint32_t            queryCount,
        const uint32_t            timestampChunk);

    VK_INLINE uint32_t EstimateMaxObjectsOnVirtualStack(size_t objectSize) const;

#if VK_ENABLE_DEBUG_BARRIERS
    void DbgCmdBarrier(bool preCmd);
#endif

    Device* const                 m_pDevice;
    CmdPool* const                m_pCmdPool;
    uint32_t                      m_queueFamilyIndex;
    Pal::QueueType                m_palQueueType;
    Pal::EngineType               m_palEngineType;
    uint32_t                      m_palDeviceMask;
    uint32_t                      m_palDeviceUsedMask;
    Pal::ICmdBuffer*              m_pPalCmdBuffers[MaxPalDevices];
    VirtualStackAllocator*        m_pStackAllocator;
    GpuEventMgr*                  m_pGpuEventMgr;

    CmdBufferRenderState          m_state; // Render state tracked during command buffer building

    VertBufBindingMgr             m_vbMgr;           // Manages current vertex buffer bindings
    StencilOpsCombiner            m_stencilCombiner; // Manages internal stencil combined state
    bool                          m_is2ndLvl;        // is this command buffer secondary or primary
    bool                          m_isRecording;
    bool                          m_needResetState;
    VkResult                      m_recordingResult; // Tracks the result of recording commands to capture OOM errors

    SqttCmdBufferState*           m_pSqttState; // Per-cmdbuf state for handling SQ thread-tracing annotations

    RenderPassInstanceState       m_renderPassInstance;

#if VK_ENABLE_DEBUG_BARRIERS
    uint32_t                      m_dbgBarrierPreCmdMask;
    uint32_t                      m_dbgBarrierPostCmdMask;
#endif
};

// =====================================================================================================================
bool CmdBuffer::IsStaticStateDifferent(
    uint32_t currentToken,
    uint32_t newToken)
{
    return ((currentToken != newToken) ||
            (currentToken == DynamicRenderStateToken));
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindMsaaState(
    Pal::ICmdBuffer*       pPalCmdBuf,
    uint32_t               deviceIdx,
    const Pal::IMsaaState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_palDeviceMask) != 0);

    if (pState != m_state.perGpuState[deviceIdx].pMsaaState)
    {
        pPalCmdBuf->CmdBindMsaaState(pState);

        m_state.perGpuState[deviceIdx].pMsaaState = pState;
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindColorBlendState(
    Pal::ICmdBuffer*             pPalCmdBuf,
    uint32_t                     deviceIdx,
    const Pal::IColorBlendState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_palDeviceMask) != 0);

    if (pState != m_state.perGpuState[deviceIdx].pColorBlendState)
    {
        pPalCmdBuf->CmdBindColorBlendState(pState);

        m_state.perGpuState[deviceIdx].pColorBlendState = pState;
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindDepthStencilState(
    Pal::ICmdBuffer*               pPalCmdBuf,
    uint32_t                       deviceIdx,
    const Pal::IDepthStencilState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_palDeviceMask) != 0);

    if (pState != m_state.perGpuState[deviceIdx].pDepthStencilState)
    {
        pPalCmdBuf->CmdBindDepthStencilState(pState);

        m_state.perGpuState[deviceIdx].pDepthStencilState = pState;
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBufferSetUserData(
    Pal::PipelineBindPoint bindPoint,
    uint32_t               firstEntry,
    uint32_t               entryCount,
    uint32_t               perDeviceStride,
    const uint32_t*        pEntryValues)
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        PalCmdBuffer(deviceIdx)->CmdSetUserData(bindPoint,
            firstEntry,
            entryCount,
            pEntryValues + (deviceIdx * perDeviceStride));
    }
}

// =====================================================================================================================
template< typename EventContainer_T >
void CmdBuffer::InsertDeviceEvents(
    const Pal::IGpuEvent**  pDestEvents,
    const EventContainer_T* pSrcEvents,
    uint32_t                index,
    uint32_t                stride
    ) const
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        pDestEvents[(deviceIdx * stride) + index] = pSrcEvents->PalEvent(deviceIdx);
    }
}

// =====================================================================================================================
Pal::ImageLayout CmdBuffer::RPGetAttachmentLayout(
    uint32_t         attachment,
    Pal::ImageAspect aspect)
{
    VK_ASSERT(aspect == Pal::ImageAspect::Color ||
              aspect == Pal::ImageAspect::Depth ||
              aspect == Pal::ImageAspect::Stencil);
    VK_ASSERT(static_cast<size_t>(aspect) < 3);
    VK_ASSERT(attachment < m_state.allGpuState.pRenderPass->GetAttachmentCount());
    VK_ASSERT(attachment < m_renderPassInstance.maxAttachmentCount);

    return m_renderPassInstance.pAttachments[attachment].aspectLayout[static_cast<size_t>(aspect)];
}

// =====================================================================================================================
void CmdBuffer::RPSetAttachmentLayout(
    uint32_t         attachment,
    Pal::ImageAspect aspect,
    Pal::ImageLayout layout)
{
    VK_ASSERT(aspect == Pal::ImageAspect::Color ||
              aspect == Pal::ImageAspect::Depth ||
              aspect == Pal::ImageAspect::Stencil);
    VK_ASSERT(static_cast<size_t>(aspect) < 3);
    VK_ASSERT(attachment < m_state.allGpuState.pRenderPass->GetAttachmentCount());
    VK_ASSERT(attachment < m_renderPassInstance.maxAttachmentCount);

    m_renderPassInstance.pAttachments[attachment].aspectLayout[static_cast<size_t>(aspect)] = layout;
}

VK_DEFINE_DISPATCHABLE(CmdBuffer);

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    const VkCommandBufferBeginInfo*             pBeginInfo);

VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers);

VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(
    VkCommandBuffer                             commandBuffer);

VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(
    VkCommandBuffer                             commandBuffer,
    VkCommandBufferResetFlags                   flags);

VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline);

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstViewport,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports);

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLineWidth(
    VkCommandBuffer                             commandBuffer,
    float                                       lineWidth);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBias(
    VkCommandBuffer                             commandBuffer,
    float                                       depthBiasConstantFactor,
    float                                       depthBiasClamp,
    float                                       depthBiasSlopeFactor);

VKAPI_ATTR void VKAPI_CALL vkCmdSetBlendConstants(
    VkCommandBuffer                             commandBuffer,
    const float                                 blendConstants[4]);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBounds(
    VkCommandBuffer                             commandBuffer,
    float                                       minDepthBounds,
    float                                       maxDepthBounds);

VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilCompareMask(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    compareMask);

VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilWriteMask(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    writeMask);

VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilReference(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    reference);

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets);

VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType);

VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets);

VKAPI_ATTR void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountAMD(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountAMD(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    x,
    uint32_t                                    y,
    uint32_t                                    z);

VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset);

VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBaseKHX(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    baseGroupX,
    uint32_t                                    baseGroupY,
    uint32_t                                    baseGroupZ,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMaskKHX(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask);

VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBase(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    baseGroupX,
    uint32_t                                    baseGroupY,
    uint32_t                                    baseGroupZ,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMask(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    srcBuffer,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferCopy*                         pRegions);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageCopy*                          pRegions);

VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageBlit*                          pRegions,
    VkFilter                                    filter);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions);

VKAPI_ATTR void VKAPI_CALL vkCmdUpdateBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                dataSize,
    const void*                                 pData);

VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                size,
    uint32_t                                    data);

VKAPI_ATTR void VKAPI_CALL vkCmdClearColorImage(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges);

VKAPI_ATTR void VKAPI_CALL vkCmdClearDepthStencilImage(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearDepthStencilValue*             pDepthStencil,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges);

VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    attachmentCount,
    const VkClearAttachment*                    pAttachments,
    uint32_t                                    rectCount,
    const VkClearRect*                          pRects);

VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage(
    VkCommandBuffer                             commandBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageResolve*                       pRegions);

VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask);

VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask);

VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers);

VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    VkDependencyFlags                           dependencyFlags,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginQuery(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    VkQueryControlFlags                         flags);

VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query);

VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount);

VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlagBits                     pipelineStage,
    VkQueryPool                                 queryPool,
    uint32_t                                    query);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyQueryPoolResults(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                stride,
    VkQueryResultFlags                          flags);

VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(
    VkCommandBuffer                             commandBuffer,
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    offset,
    uint32_t                                    size,
    const void*                                 pValues);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents);

VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer                             commandBuffer,
    VkSubpassContents                           contents);

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers);

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT(
    VkCommandBuffer                             commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleLocationsEXT(
    VkCommandBuffer                             commandBuffer,
    const VkSampleLocationsInfoEXT*             pSampleLocationsInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarkerAMD(
    VkCommandBuffer         commandBuffer,
    VkPipelineStageFlagBits pipelineStage,
    VkBuffer                dstBuffer,
    VkDeviceSize            dstOffset,
    uint32_t                marker);

} // namespace entry

} // namespace vk

#endif /* __VK_CMDBUFFER_H__ */
