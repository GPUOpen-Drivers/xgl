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
#include "include/vk_device.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_utils.h"

#include "include/barrier_policy.h"
#include "include/graphics_pipeline_common.h"
#include "include/internal_mem_mgr.h"
#include "include/virtual_stack_mgr.h"

#include "renderpass/renderpass_builder.h"

#if VKI_RAY_TRACING
#endif

#include "debug_printf.h"
#include "palCmdBuffer.h"
#include "palDequeImpl.h"
#include "palGpuMemory.h"
#include "palLinearAllocator.h"
#include "palPipeline.h"
#include "palQueue.h"
#include "palVector.h"

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
class ApiCmdBuffer;
class Framebuffer;
class GraphicsPipeline;
class Image;
class ImageView;
class Queue;
class RenderPass;
class TimestampQueryPool;
class SqttCmdBufferState;
class QueryPool;

#if VKI_RAY_TRACING
class RayTracingPipeline;
class AccelerationStructureQueryPool;
#endif

constexpr uint8_t  DefaultStencilOpValue      = 1;
constexpr uint8_t  DefaultRefPicIndexValue    = 0xFF;
constexpr uint8_t  DefaultIndex7BitsValue     = 0x7F;
constexpr uint8_t  DefaultTidValue            = 0xFF;
constexpr uint8_t  AssociatedFlag             = 31;
constexpr uint32_t DefaultAssociatedFlagValue = (1 << AssociatedFlag);

// Internal API pipeline binding points
enum PipelineBindPoint
{
    PipelineBindCompute = 0,
    PipelineBindGraphics,
#if VKI_RAY_TRACING
    PipelineBindRayTracing,
#endif
    PipelineBindCount
};

// Dynamic bind-time info (wave limits, etc.) for either graphics or compute-based pipelines
union PipelineDynamicBindInfo
{
    Pal::DynamicComputeShaderInfo   cs;
    Pal::DynamicGraphicsShaderInfos gfx;
};

// Internal buffer for dynamic vertex input
struct DynamicVertexInputInternalData
{
    Pal::gpusize gpuAddress[MaxPalDevices];
};

typedef Util::HashMap<uint64_t, DynamicVertexInputInternalData, PalAllocator, Util::JenkinsHashFunc>
    UberFetchShaderInternalDataMap;

// This structure contains information about currently written user data entries within the command buffer
struct PipelineBindState
{
    // Cached copy of the user data layout from the current pipeline's layout
    UserDataLayout userDataLayout;
    // High-water mark of the largest number of bound sets
    uint32_t boundSetCount;
    // High-water mark of the largest number of pushed constants
    uint32_t pushedConstCount;
    // Currently pushed constant values (relative to an base = 0)
    uint32_t pushConstData[MaxPushConstRegCount];
    // Dynamic info (wave limits, etc.)
    PipelineDynamicBindInfo dynamicBindInfo;
    // The command buffer's push descriptor set state
    VkDescriptorSet pushDescriptorSet;
    void*           pPushDescriptorSetMemory;
    size_t          pushDescriptorSetMaxSize;

    // Internal data of dynamic vertex input
    DynamicVertexInputInternalData* pVertexInputInternalData;
    // Whether dynamic vertex input is enabled in current pipeline
    bool            hasDynamicVertexInput;
};

union DirtyGraphicsState
{
    struct
    {
        uint32 viewport                :  1;
        uint32 scissor                 :  1;
        uint32 depthStencil            :  1;
        uint32 rasterState             :  1;
        uint32 inputAssembly           :  1;
        uint32 stencilRef              :  1;
        uint32 vrs                     :  1;
        uint32 samplePattern           :  1;
        uint32 colorBlend              :  1;
        uint32 msaa                    :  1;
        uint32 pipeline                :  1;
        uint32 reserved                : 21;
    };

    uint32 u32All;
};

struct DescriptorBuffers
{
    VkDeviceSize     offset;
    uint32           baseAddrNdx;
};

struct DescBufBinding
{
    VkDeviceAddress     baseAddr[MaxDescriptorSets];
};

struct DynamicDepthStencil
{
    Pal::IDepthStencilState* pPalDepthStencil[MaxPalDevices];
};

struct DynamicColorBlend
{
    Pal::IColorBlendState* pPalColorBlend[MaxPalDevices];
};

struct DynamicMsaa
{
    Pal::IMsaaState* pPalMsaa[MaxPalDevices];
};

// Members of CmdBufferRenderState that are different for each GPU
struct PerGpuRenderState
{
    Pal::ScissorRectParams scissor;
    Pal::ViewportParams    viewport;

    // Any members added to this structure may need to be cleared in CmdBuffer::ResetState().
    const Pal::IMsaaState*          pMsaaState;
    const Pal::IColorBlendState*    pColorBlendState;
    const Pal::IDepthStencilState*  pDepthStencilState;

    // The max stack size required by the pipelines referenced in this command buffer
    uint32_t maxPipelineStackSize;

    // VB bindings in source non-SRD form
    Pal::BufferViewInfo vbBindings[Pal::MaxVertexBuffers];

    // Currently bound descriptor sets and dynamic offsets (relative to base = 00).
    // NOTE: Memory will not be allocated for all PipelineBindCount/MaxBindingRegCount if known to be inaccessible.
    // This applies to the last GPU only in order to preserve PerGpu indexing of this render state struct.
    uint32_t setBindingData[PipelineBindCount][MaxBindingRegCount];

    // DO NOT ADD ANYTHING HERE! Additional members must be placed above setBindingData so it remains the last member.
};

struct DynamicRenderingAttachments
{
    VkResolveModeFlagBits resolveMode;
    const ImageView*      pImageView;
    Pal::ImageLayout      imageLayout;
    const ImageView*      pResolveImageView;
    Pal::ImageLayout      resolveImageLayout;
    VkFormat              attachmentFormat;
    VkSampleCountFlagBits rasterizationSamples;
};

struct DynamicRenderingInstance
{
    uint32_t                    viewMask;
    uint32_t                    renderAreaCount;
    Pal::Rect                   renderArea[MaxPalDevices];
    bool                        enableResolveTarget;
    uint32_t                    colorAttachmentCount;
    DynamicRenderingAttachments colorAttachments[Pal::MaxColorTargets];
    DynamicRenderingAttachments depthAttachment;
    DynamicRenderingAttachments stencilAttachment;
};

// Members of CmdBufferRenderState that are the same for each GPU
struct AllGpuRenderState
{
    const RenderPass*             pRenderPass;

    // The Imageless Frambuffer extension allows setting this at RenderPassBind
    Framebuffer*             pFramebuffer;

    // Dirty bits indicate which state should be validated. It assumed viewport/scissor in perGpuStates will likely be
    // changed for all GPUs if it is changed for any GPU. Put DirtyGraphicsState management here will be easier to manage.
    DirtyGraphicsState dirtyGraphics;

    // Value of VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
    // defined by the last bound GraphicsPipeline, which was not nullptr.
    bool viewIndexFromDeviceIndex;

    DynamicRenderingInstance         dynamicRenderingInstance;

// =====================================================================================================================
// The first part of the structure will be cleared with a memset in CmdBuffer::ResetState().
// The second part of the structure contains the larger members that are selectively reset in CmdBuffer::ResetState().
// =====================================================================================================================
    // Keep pipelineState as the first member of the section that is selectively reset.  It is used to compute how large
    // the first part is for the memset in CmdBuffer::ResetState().
    PipelineBindState       pipelineState[PipelineBindCount];

    uint64_t                      boundGraphicsPipelineHash;
    const GraphicsPipeline*       pGraphicsPipeline;
    const ComputePipeline*        pComputePipeline;
#if VKI_RAY_TRACING
    const RayTracingPipeline*     pRayTracingPipeline;
#endif

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
        uint32_t lineStippleState;
        uint32_t depthBiasState;
        uint32_t blendConst;
        uint32_t depthBounds;
        uint32_t viewports;
        uint32_t scissorRect;
        uint32_t fragmentShadingRate;
    } staticTokens;

    // Which Vulkan PipelineBindPoint currently owns the state of each PAL pipeline bind point.  This is
    // relevant because e.g. multiple Vulkan pipeline bind points are implemented as compute pipelines and used through
    // the same PAL pipeline bind point.
    PipelineBindPoint       palToApiPipeline[static_cast<size_t>(Pal::PipelineBindPoint::Count)];

    Pal::LineStippleStateParams      lineStipple;
    Pal::TriangleRasterStateParams   triangleRasterState;
    Pal::StencilRefMaskParams        stencilRefMasks;
    Pal::InputAssemblyStateParams    inputAssemblyState;
    Pal::DepthStencilStateCreateInfo depthStencilCreateInfo;
    Pal::VrsRateParams               vrsRate;
    Pal::ColorBlendStateCreateInfo   colorBlendCreateInfo;
    Pal::MsaaStateCreateInfo         msaaCreateInfo;

    uint32_t                         colorWriteEnable; // Per target mask from vkCmdSetColorWriteEnableEXT or pipeline
    uint32_t                         colorWriteMask;

    SamplePattern                    samplePattern;
    VkBool32                         sampleLocationsEnable;
    DescBufBinding*                  pDescBufBinding;
    VkLogicOp                        logicOp;
    VkBool32                         logicOpEnable;
    float                            minSampleShading;
};

// State tracked during a render pass instance when building a command buffer.
struct RenderPassInstanceState
{
    // Per-attachment instance state
    struct AttachmentState
    {
        Pal::ImageLayout planeLayout[Pal::MaxNumPlanes];  // Per-plane PAL layout
        VkClearValue     clearValue;                      // Specified load-op clear value for this attachment
        SamplePattern    initialSamplePattern;            // Initial sample pattern at first layout transition of
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

struct TransformFeedbackState
{
    Pal::BindStreamOutTargetParams  params;

    struct
    {
        VkBuffer        buffer;
        VkDeviceSize    size;
        VkDeviceSize    offset;
    } bufferInfo[Pal::MaxStreamOutTargets];

    uint32_t            bindMask;
    bool                enabled;
};

enum AcquireReleaseMode
{
    Release = 0,
    Acquire,
    ReleaseThenAcquire
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

    void BindIndexBuffer(
        VkBuffer                                    buffer,
        VkDeviceSize                                offset,
        VkDeviceSize                                size,
        VkIndexType                                 indexType);

    void BindVertexBuffers(
        uint32_t                                    firstBinding,
        uint32_t                                    bindingCount,
        const VkBuffer*                             pBuffers,
        const VkDeviceSize*                         pOffsets,
        const VkDeviceSize*                         pSizes,
        const VkDeviceSize*                         pStrides);

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

    void DrawMeshTasks(
        uint32_t                                    x,
        uint32_t                                    y,
        uint32_t                                    z);

    template<bool useBufferCount>
    void DrawMeshTasksIndirect(
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

    template<typename BufferCopyType>
    void CopyBuffer(
        VkBuffer                                    srcBuffer,
        VkBuffer                                    destBuffer,
        uint32_t                                    regionCount,
        const BufferCopyType*                       pRegions);

    template<typename ImageCopyType>
    void CopyImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const ImageCopyType*                        pRegions);

    template<typename ImageBlitType>
    void BlitImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const ImageBlitType*                        pRegions,
        VkFilter                                    filter);

    template<typename BufferImageCopyType>
    void CopyBufferToImage(
        VkBuffer                                    srcBuffer,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    regionCount,
        const BufferImageCopyType*                  pRegions);

    template<typename BufferImageCopyType>
    void CopyImageToBuffer(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkBuffer                                    destBuffer,
        uint32_t                                    regionCount,
        const BufferImageCopyType*                  pRegions);

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

    void ClearDynamicRenderingImages(
        uint32_t                 attachmentCount,
        const VkClearAttachment* pAttachments,
        uint32_t                 rectCount,
        const VkClearRect*       pRects);

    void ClearDynamicRenderingBoundAttachments(
        uint32_t                                    attachmentCount,
        const VkClearAttachment*                    pAttachments,
        uint32_t                                    rectCount,
        const VkClearRect*                          pRects);

    template<typename ImageResolveType>
    void ResolveImage(
        VkImage                                     srcImage,
        VkImageLayout                               srcImageLayout,
        VkImage                                     destImage,
        VkImageLayout                               destImageLayout,
        uint32_t                                    rectCount,
        const ImageResolveType*                     pRects);

    void SetViewport(
        uint32_t                                    firstViewport,
        uint32_t                                    viewportCount,
        const VkViewport*                           pViewports);

    void SetViewportWithCount(
        uint32_t                                    viewportCount,
        const VkViewport*                           pViewports);

    void SetAllViewports(
        const Pal::ViewportParams&                  params,
        uint32_t                                    staticToken);

    void SetScissor(
        uint32_t                                    firstScissor,
        uint32_t                                    scissorCount,
        const VkRect2D*                             pScissors);

    void SetScissorWithCount(
        uint32_t                                    scissorCount,
        const VkRect2D*                             pScissors);

    void SetAllScissors(
        const Pal::ScissorRectParams&               params,
        uint32_t                                    staticToken);

    void SetDepthTestEnableEXT(
        VkBool32                                    depthTestEnable);

    void SetDepthWriteEnableEXT(
        VkBool32                                    depthWriteEnable);

    void SetDepthCompareOpEXT(
        VkCompareOp                                 depthCompareOp);

    void SetDepthBoundsTestEnableEXT(
        VkBool32                                    depthBoundsTestEnable);

    void SetStencilTestEnableEXT(
        VkBool32                                    stencilTestEnable);

    void SetStencilOpEXT(
        VkStencilFaceFlags                          faceMask,
        VkStencilOp                                 failOp,
        VkStencilOp                                 passOp,
        VkStencilOp                                 depthFailOp,
        VkCompareOp                                 compareOp);

    void SetCullModeEXT(
        VkCullModeFlags                             cullMode);

    void SetFrontFaceEXT(
        VkFrontFace                                 frontFace);

    void SetPrimitiveTopologyEXT(
        VkPrimitiveTopology                         primitiveTopology);

    void SetLineStippleEXT(
        uint32_t                                    lineStippleFactor,
        uint16_t                                    lineStipplePattern);

    void SetColorWriteEnableEXT(
        uint32_t                                    attachmentCount,
        const VkBool32*                             pColorWriteEnables);

    void SetRasterizerDiscardEnableEXT(
        VkBool32                                   rasterizerDiscardEnable);

    void SetPrimitiveRestartEnableEXT(
        VkBool32                                   primitiveRestartEnable);

    void SetDepthBiasEnableEXT(
        VkBool32                                   depthBiasEnable);

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

    void SetViewInstanceMask(
        uint32_t                                    deviceMask);

    void SetStencilCompareMask(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilCompareMask);

    void SetStencilWriteMask(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilWriteMask);

    void SetStencilReference(
        VkStencilFaceFlags                          faceMask,
        uint32_t                                    stencilReference);

    void SetVertexInput(
        uint32_t                                     vertexBindingDescriptionCount,
        const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
        uint32_t                                     vertexAttributeDescriptionCount,
        const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);

    void SetColorBlendEnable(
        uint32_t                            firstAttachment,
        uint32_t                            attachmentCount,
        const VkBool32*                     pColorBlendEnables);

    void SetColorBlendEquation(
        uint32_t                            firstAttachment,
        uint32_t                            attachmentCount,
        const VkColorBlendEquationEXT*      pColorBlendEquations);

    void SetRasterizationSamples(
        VkSampleCountFlagBits               rasterizationSamples);

    void SetSampleMask(
        VkSampleCountFlagBits               samples,
        const VkSampleMask*                 pSampleMask);

    void SetConservativeRasterizationMode(
        VkConservativeRasterizationModeEXT  conservativeRasterizationMode);

    void SetExtraPrimitiveOverestimationSize(
        float                               extraPrimitiveOverestimationSize);

    void SetLineStippleEnable(
        VkBool32                            stippledLineEnable);

    void SetPolygonMode(
        VkPolygonMode                       polygonMode);

    void SetProvokingVertexMode(
        VkProvokingVertexModeEXT            provokingVertexMode);

    void SetColorWriteMask(
        uint32_t                            firstAttachment,
        uint32_t                            attachmentCount,
        const  VkColorComponentFlags*       pColorWriteMasks);

    void SetSampleLocationsEnable(
        VkBool32                            sampleLocationsEnable);

    void SetLogicOp(
        VkLogicOp                           logicOp);

    void SetLogicOpEnable(
        VkBool32                            logicOpEnable);

    void SetLineRasterizationMode(
        VkLineRasterizationModeEXT          lineRasterizationMode);

    void SetTessellationDomainOrigin(
        VkTessellationDomainOrigin          domainOrigin);

    void SetDepthClampEnable(
        VkBool32                            depthClampEnable);

    void SetAlphaToCoverageEnable(
        VkBool32                            alphaToCoverageEnable);

    void SetDepthClipEnable(
        VkBool32                            depthClipEnable);

    void SetDepthClipNegativeOneToOne(
        VkBool32                            negativeOneToOne);

    void SetEvent(
        VkEvent                                     event,
        PipelineStageFlags                          stageMask);

    void SetEvent2(
        VkEvent                                     event,
        const VkDependencyInfoKHR*                  pDependencyInfo);

    void ResetEvent(
        VkEvent                                     event,
        PipelineStageFlags                          stageMask);

    void WaitEvents(
        uint32_t                                    eventCount,
        const VkEvent*                              pEvents,
        PipelineStageFlags                          srcStageMask,
        PipelineStageFlags                          dstStageMask,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers);

    void WaitEvents2(
        uint32_t                                    eventCount,
        const VkEvent*                              pEvents,
        const VkDependencyInfoKHR*                  pDependencyInfos);

    void WaitEventsSync2ToSync1(
        uint32_t                                    eventCount,
        const VkEvent*                              pEvents,
        uint32_t                                    dependencyCount,
        const VkDependencyInfoKHR*                  pDependencyInfos);

    void PipelineBarrier(
        PipelineStageFlags                          srcStageMask,
        PipelineStageFlags                          dstStageMask,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers);

    void PipelineBarrier2(
        const VkDependencyInfoKHR*                  pDependencyInfo);

    void PipelineBarrierSync2ToSync1(
        const VkDependencyInfoKHR*                  pDependencyInfo);

    void BeginRendering(
        const VkRenderingInfoKHR*                   pRenderingInfo);

    void EndRendering();

    void PostDrawPreResolveSync();

    void BeginQueryIndexed(
        VkQueryPool                                 queryPool,
        uint32_t                                    query,
        VkQueryControlFlags                         flags,
        uint32_t                                    index);

    void EndQueryIndexed(
        VkQueryPool                                 queryPool,
        uint32_t                                    query,
        uint32_t                                    index);

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
        PipelineStageFlags                          pipelineStage,
        const TimestampQueryPool*                   pQueryPool,
        uint32_t                                    query);

    void SetSampleLocations(
        const VkSampleLocationsInfoEXT* pSampleLocationsInfo);

    void BeginRenderPass(
        const VkRenderPassBeginInfo* pRenderPassBegin,
        VkSubpassContents            contents);

    void NextSubPass(
        VkSubpassContents            contents);

    void EndRenderPass();

    void PushConstants(
        VkPipelineLayout                            layout,
        VkShaderStageFlags                          stageFlags,
        uint32_t                                    start,
        uint32_t                                    length,
        const void*                                 values);

    template <size_t imageDescSize,
              size_t samplerDescSize,
              size_t bufferDescSize,
              uint32_t numPalDevices>
    void PushDescriptorSetKHR(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    set,
        uint32_t                                    descriptorWriteCount,
        const VkWriteDescriptorSet*                 pDescriptorWrites);

    template <uint32_t numPalDevices>
    void PushDescriptorSetWithTemplateKHR(
        VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
        VkPipelineLayout                            layout,
        uint32_t                                    set,
        const void*                                 pData);

    void WriteBufferMarker(
        PipelineStageFlags      pipelineStage,
        VkBuffer                dstBuffer,
        VkDeviceSize            dstOffset,
        uint32_t                marker);

    void BindTransformFeedbackBuffers(
        uint32_t            firstBinding,
        uint32_t            bindingCount,
        const VkBuffer*     pBuffers,
        const VkDeviceSize* pOffsets,
        const VkDeviceSize* pSizes);

    void BeginTransformFeedback(
        uint32_t                firstCounterBuffer,
        uint32_t                counterBufferCount,
        const VkBuffer*         pCounterBuffers,
        const VkDeviceSize*     pCounterBufferOffsets);

    void EndTransformFeedback(
        uint32_t            firstCounterBuffer,
        uint32_t            counterBufferCount,
        const VkBuffer*     pCounterBuffers,
        const VkDeviceSize* pCounterBufferOffsets);

    void DrawIndirectByteCount(
        uint32_t        instanceCount,
        uint32_t        firstInstance,
        VkBuffer        counterBuffer,
        VkDeviceSize    counterBufferOffset,
        uint32_t        counterOffset,
        uint32_t        vertexStride);

    void CmdSetPerDrawVrsRate(
        const VkExtent2D*                        pFragmentSize,
        const VkFragmentShadingRateCombinerOpKHR combinerOps[2]);

    void CmdBeginConditionalRendering(const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin);
    void CmdEndConditionalRendering();

    void CmdDebugMarkerBegin(
        const VkDebugMarkerMarkerInfoEXT* pMarkerInfo);

    void CmdDebugMarkerEnd();

    void CmdBeginDebugUtilsLabel(
        const VkDebugUtilsLabelEXT* pLabelInfo);

    void CmdEndDebugUtilsLabel();

    void SetDeviceMask(uint32_t deviceMask)
    {
        // Ensure we are enabling valid devices within the group
        VK_ASSERT((m_pDevice->GetPalDeviceMask() & deviceMask) == deviceMask);

        // Ensure disabled devices are not enabled during recording
        VK_ASSERT(((m_cbBeginDeviceMask ^ deviceMask) & deviceMask) == 0);

        // If called inside a render pass, ensure devices outside of render pass device mask are not enabled
        VK_ASSERT((m_allGpuState.pRenderPass == nullptr) ||
                  (((m_rpDeviceMask ^ deviceMask) & deviceMask) == 0));

        m_curDeviceMask = deviceMask;
    }

    uint32_t GetDeviceMask() const
    {
        return m_curDeviceMask;
    }

   void SetRpDeviceMask(uint32_t deviceMask)
    {
        VK_ASSERT(deviceMask != 0);

        // Ensure render pass device mask is within the command buffer's initial device mask
        VK_ASSERT(((m_cbBeginDeviceMask ^ deviceMask) & deviceMask) == 0);

        m_rpDeviceMask = deviceMask;
    }

    uint32_t GetRpDeviceMask() const
    {
        return m_rpDeviceMask;
    }

    uint32_t GetBeginDeviceMask() const
    {
        return m_cbBeginDeviceMask;
    }

    bool IsProtected() const
    {
        return m_pCmdPool->IsProtected();
    }

    bool IsBackupBufferUsed() const
    {
        return m_flags.useBackupBuffer;
    }

    bool IsSecondaryLevel() const
    {
        return m_flags.is2ndLvl;
    }

    VkResult Destroy(void);

    VK_FORCEINLINE Device* VkDevice(void) const
        { return m_pDevice; }

    VK_FORCEINLINE Instance* VkInstance(void) const
        { return m_pDevice->VkInstance(); }

    Pal::ICmdBuffer* PalCmdBuffer(
            int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalCmdBuffers[idx];
    }

    Pal::ICmdBuffer* BackupPalCmdBuffer(
        uint32_t idx) const
    {
        VK_ASSERT(idx < static_cast<uint32_t>(MaxPalDevices));
        return m_pBackupPalCmdBuffers[idx];
    }

    VK_FORCEINLINE uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }
    VK_FORCEINLINE Pal::QueueType GetPalQueueType() const { return m_palQueueType; }
    VK_FORCEINLINE Pal::EngineType GetPalEngineType() const { return m_palEngineType; }

    VK_FORCEINLINE VirtualStackAllocator* GetStackAllocator() { return m_pStackAllocator; }

    void TranslateBarrierInfoToAcqRel(
        const Pal::BarrierInfo& barrierInfo,
        uint32_t                deviceMask);

    void PalCmdBarrier(
        const Pal::BarrierInfo& info,
        uint32_t                deviceMask);

    void PalCmdBarrier(
        Pal::BarrierInfo*             pInfo,
        Pal::BarrierTransition* const pTransitions,
        const Image** const           pTransitionImages,
        uint32_t                      deviceMask);

    void PalCmdReleaseThenAcquire(
        const Pal::AcquireReleaseInfo& info,
        uint32_t                       deviceMask);

    void PalCmdReleaseThenAcquire(
        Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
        Pal::MemBarrier* const   pMemoryBarriers,
        const Buffer** const     ppBuffers,
        Pal::ImgBarrier* const   pImageBarriers,
        const Image** const      ppImages,
        uint32_t                 deviceMask);

    void PalCmdAcquire(
        Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
        uint32_t                 eventCount,
        const VkEvent*           pEvents,
        Pal::MemBarrier* const   pBufferBarriers,
        const Buffer** const     ppBuffers,
        Pal::ImgBarrier* const   pImageBarriers,
        const Image** const      ppImages,
        VirtualStackFrame*       pVirtStackFrame,
        uint32_t                 deviceMask);

    void PalCmdRelease(
        Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
        uint32_t                 eventCount,
        const VkEvent*           pEvents,
        Pal::MemBarrier* const   pBufferBarriers,
        const Buffer** const     ppBuffers,
        Pal::ImgBarrier* const   pImageBarriers,
        const Image** const      ppImages,
        uint32_t                 deviceMask);

    Pal::Result PalCmdBufferBegin(
            const Pal::CmdBufferBuildInfo& cmdInfo);

    Pal::Result PalCmdBufferEnd();
    Pal::Result PalCmdBufferReset(bool returnGpuMemory);

    void PalCmdBufferDestroy();

    void PalCmdBindIndexData(
                Buffer* pBuffer,
                Pal::gpusize offset,
                Pal::IndexType indexType,
                Pal::gpusize bufferSize);

    void PalCmdUnbindIndexData(Pal::IndexType indexType);

    void PalCmdDraw(
        uint32_t firstVertex,
        uint32_t vertexCount,
        uint32_t firstInstance,
        uint32_t instanceCount,
        uint32_t drawId);

    void PalCmdDrawIndexed(
        uint32_t firstIndex,
        uint32_t indexCount,
        int32_t  vertexOffset,
        uint32_t firstInstance,
        uint32_t instanceCount,
        uint32_t drawId);

    void PalCmdDrawMeshTasks(
        uint32_t x,
        uint32_t y,
        uint32_t z);

    template<bool useBufferCount>
    void PalCmdDrawMeshTasksIndirect(
        VkBuffer     buffer,
        VkDeviceSize offset,
        uint32_t     count,
        uint32_t     stride,
        VkBuffer     countBuffer,
        VkDeviceSize countOffset);

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
        VkImageLayout         srcImageLayout,
        const Image* const    pDstImage,
        VkImageLayout         destImageLayout,
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
        const Image&               image,
        Pal::ImageLayout           imageLayout,
        const Pal::ClearColor&     color,
        const Pal::SwizzledFormat& clearFormat,
        Pal::uint32                rangeCount,
        const Pal::SubresRange*    pRanges,
        Pal::uint32                boxCount,
        const Pal::Box*            pBoxes,
        Pal::uint32                flags);

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

    void PalCmdResolveImage(
        const Image&                   srcImage,
        Pal::ImageLayout               srcImageLayout,
        const Image&                   dstImage,
        Pal::ImageLayout               dstImageLayout,
        Pal::ResolveMode               resolveMode,
        uint32_t                       regionCount,
        const Pal::ImageResolveRegion* pRegions,
        uint32_t                       deviceMask);

    bool PreBltBindMsaaState(const Image& image);

    void PostBltRestoreMsaaState(bool bltMsaaState);

    void PalCmdBindMsaaStates(const Pal::IMsaaState* const * pStates);

    inline void PalCmdBindMsaaState(
        Pal::ICmdBuffer*       pPalCmdBuf,
        uint32_t               deviceIdx,
        const Pal::IMsaaState* pState);

    inline void PalCmdBindColorBlendState(
        Pal::ICmdBuffer*             pPalCmdBuf,
        uint32_t                     deviceIdx,
        const Pal::IColorBlendState* pState);

    inline void PalCmdBindDepthStencilState(
        Pal::ICmdBuffer*               pPalCmdBuf,
        uint32_t                       deviceIdx,
        const Pal::IDepthStencilState* pState);

    void PalCmdSetMsaaQuadSamplePattern(
        uint32_t                           numSamplesPerPixel,
        const  Pal::MsaaQuadSamplePattern& quadSamplePattern);

    inline void PalCmdBufferSetUserData(
        Pal::PipelineBindPoint bindPoint,
        uint32_t               firstEntry,
        uint32_t               entryCount,
        uint32_t               perDeviceStride,
        const uint32_t*        pEntryValues);

    void PalCmdSuspendPredication(
        bool suspend);

    template< typename EventContainer_T >
    void InsertDeviceEvents(
        const Pal::IGpuEvent**  pDestEvents,
        const EventContainer_T* pSrcEvents,
        uint32_t                index,
        uint32_t                stride) const;

    VK_FORCEINLINE uint32_t NumPalDevices() const
        { return m_numPalDevices; }

    uint32_t NumDeviceEvents(uint32_t numEvents) const
        { return m_numPalDevices * numEvents; }

#if VK_ENABLE_DEBUG_BARRIERS
    void DbgBarrierPreCmd(uint64_t cmd)
    {
        if (m_dbgBarrierPreCmdMask & (cmd))
        {
            DbgCmdBarrier(true);
        }
    }
    void DbgBarrierPostCmd(uint64_t cmd)
    {
        if (m_dbgBarrierPostCmdMask & (cmd))
        {
            DbgCmdBarrier(false);
        }
    }
#else
    void DbgBarrierPreCmd(uint64_t cmd) {}
    void DbgBarrierPostCmd(uint64_t cmd) {}
#endif

    SqttCmdBufferState* GetSqttState()
        { return m_pSqttState; }

    static bool IsStaticStateDifferent(
        uint32_t currentToken,
        uint32_t newToken)
    {
        return ((currentToken != newToken) || (currentToken == DynamicRenderStateToken));
    }

    static PFN_vkCmdBindDescriptorSets GetCmdBindDescriptorSetsFunc(const Device* pDevice);

    static PFN_vkCmdPushDescriptorSetKHR GetCmdPushDescriptorSetKHRFunc(const Device* pDevice);
    static PFN_vkCmdPushDescriptorSetWithTemplateKHR GetCmdPushDescriptorSetWithTemplateKHRFunc(const Device* pDevice);

#if VKI_RAY_TRACING
    void BuildAccelerationStructures(
        uint32                                                  infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos,
        const VkDeviceAddress*                                  pIndirectDeviceAddresses,
        const uint32*                                           pIndirectStrides,
        const uint32* const*                                    ppMaxPrimitiveCounts);

    void CopyAccelerationStructure(
        const VkCopyAccelerationStructureInfoKHR*   pInfo);

    void CopyAccelerationStructureToMemory(
        const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo);

    void CopyMemoryToAccelerationStructure(
        const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo);

    void WriteAccelerationStructuresProperties(
        uint32_t                                    accelerationStructureCount,
        const VkAccelerationStructureKHR*           pAccelerationStructures,
        VkQueryType                                 queryType,
        VkQueryPool                                 queryPool,
        uint32_t                                    firstQuery);

    void TraceRays(
        const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
        uint32_t                        width,
        uint32_t                        height,
        uint32_t                        depth);

    void TraceRaysIndirect(
        GpuRt::ExecuteIndirectArgType          indirectArgType,
        const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
        VkDeviceAddress                        indirectDeviceAddress);

    VkResult GetRayTracingIndirectMemory(
        gpusize                             size,
        InternalMemory**                    ppInternalMemory);

    const RayTracingPipeline* GetBoundRayTracingPipeline() const
        { return m_allGpuState.pRayTracingPipeline; }

    void FreeRayTracingIndirectMemory();

    void SetRayTracingPipelineStackSize(uint32_t pipelineStackSize);

#endif

    void BindDescriptorBuffers(
        uint32_t                                    bufferCount,
        const VkDescriptorBufferBindingInfoEXT*     pBindingInfos);

    void SetDescriptorBufferOffsets(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        const uint32_t*                             pBufferIndices,
        const VkDeviceSize*                         pOffsets);

    void BindDescriptorBufferEmbeddedSamplers(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    set);

    CmdPool* GetCmdPool() const { return m_pCmdPool; }

    PerGpuRenderState* PerGpuState(uint32 deviceIdx)
    {
        PerGpuRenderState* pPerGpuState =
            static_cast<PerGpuRenderState*>(Util::VoidPtrInc(this, sizeof(*this)));

        return &pPerGpuState[deviceIdx];
    }

    const PerGpuRenderState* PerGpuState(uint32 deviceIdx) const
    {
        const PerGpuRenderState* pPerGpuState =
            static_cast<const PerGpuRenderState*>(Util::VoidPtrInc(this, sizeof(*this)));

        return &pPerGpuState[deviceIdx];
    }

    AllGpuRenderState* RenderState() { return &m_allGpuState; }

    // Get a safe number of objects that can be allocated by the virtual stack frame allocator without risking OOM error.
    uint32_t EstimateMaxObjectsOnVirtualStack(size_t objectSize) const
    {
        // Return at least 1 and use only 50% of the remaining space.
        return 1 + static_cast<uint32_t>((m_pStackAllocator->Remaining() / objectSize) >> 1);
    }

#if VKI_RAY_TRACING

    bool HasRayTracing() const { return m_flags.hasRayTracing; }
#endif

    DebugPrintf* GetDebugPrintf()
    {
        return &m_debugPrintf;
    }
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);

    uint32 GetHevcDbpIndex(const uint8_t* pRefPicList, uint32 dpbSlot);

    void ValidateGraphicsStates();

    void ValidateSamplePattern(uint32_t sampleCount, SamplePattern* pSamplePattern);

    CmdBuffer(
        Device*                         pDevice,
        CmdPool*                        pCmdPool,
        uint32_t                        queueFamilyIndex);

    VkResult Initialize(
        void*                           pPalMem,
        const Pal::CmdBufferCreateInfo& createInfo);

    void SyncIndirectCopy(
        Pal::ICmdBuffer*                pCmdBuffer);

    Pal::Result BackupInitialize(
        const Pal::CmdBufferCreateInfo& createInfo);
    void SwitchToBackupCmdBuffer();
    void RestoreFromBackupCmdBuffer();

    void ResetPipelineState();

    void ResetState();

    void QueryCopy(
        const QueryPool*   pBasePool,
        const Buffer*      pDestBuffer,
        uint32_t           firstQuery,
        uint32_t           queryCount,
        VkDeviceSize       destOffset,
        VkDeviceSize       destStride,
        VkQueryResultFlags flags);

    void CalcCounterBufferAddrs(
        uint32_t            firstCounterBuffer,
        uint32_t            counterBufferCount,
        const VkBuffer*     pCounterBuffers,
        const VkDeviceSize* pCounterBufferOffsets,
        uint64_t*           counterBufferAddr,
        uint32_t            deviceIdx);

    void FlushBarriers(
        Pal::BarrierInfo*              pBarrier,
        Pal::BarrierTransition* const  pTransitions,
        const Image**                  pTransitionImages,
        uint32_t                       mainTransitionCount);

    void ExecuteBarriers(
        VirtualStackFrame*           pVirtStackFrame,
        uint32_t                     memBarrierCount,
        const VkMemoryBarrier*       pMemoryBarriers,
        uint32_t                     bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers,
        uint32_t                     imageMemoryBarrierCount,
        const VkImageMemoryBarrier*  pImageMemoryBarriers,
        Pal::BarrierInfo*            pBarrier);

    void ExecuteReleaseThenAcquire(
        PipelineStageFlags           srcStageMask,
        PipelineStageFlags           dstStageMask,
        uint32_t                     memBarrierCount,
        const VkMemoryBarrier*       pMemoryBarriers,
        uint32_t                     bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers,
        uint32_t                     imageMemoryBarrierCount,
        const VkImageMemoryBarrier*  pImageMemoryBarriers);

    void ExecuteAcquireRelease(
        uint32_t                     eventCount,
        const VkEvent*               pEvents,
        uint32_t                     dependencyCount,
        const VkDependencyInfoKHR*   pDependencyInfos,
        AcquireReleaseMode           acquireReleaseMode,
        uint32_t                     rgpBarrierReasonType);

    enum RebindUserDataFlag : uint32_t
    {
        RebindUserDataDescriptorSets = 0x1,
        RebindUserDataPushConstants  = 0x2,
        RebindUberFetchInternalMem   = 0x4,
        RebindUserDataAll            = ~0u
    };

    typedef uint32_t RebindUserDataFlags;

    RebindUserDataFlags SwitchUserDataLayouts(
        PipelineBindPoint      apiBindPoint,
        const UserDataLayout*  pUserDataLayout);

    template<PipelineBindPoint bindPoint, bool fromBindPipeline>
    void RebindPipeline();

    void RebindUserData(
        PipelineBindPoint      apiBindPoint,
        Pal::PipelineBindPoint palBindPoint,
        RebindUserDataFlags    flags);

    void RPBeginSubpass();
    void RPEndSubpass();
    void RPResolveAttachments(uint32_t count, const RPResolveInfo* pResolves);
    void RPSyncPoint(const RPSyncPointInfo& syncPoint, VirtualStackFrame* pVirtStack);
    void RPSyncPointLegacy(const RPSyncPointInfo& syncPoint, VirtualStackFrame* pVirtStack);
    void RPLoadOpClearColor(uint32_t count, const RPLoadOpClearInfo* pClears);
    void RPLoadOpClearDepthStencil(uint32_t count, const RPLoadOpClearInfo* pClears);
    void RPBindTargets(const RPBindTargetsInfo& targets);
    void RPSyncPostLoadOpColorClear();

    void BindTargets(
        const VkRenderingInfoKHR*                              pRenderingInfo,
        const VkRenderingFragmentShadingRateAttachmentInfoKHR* pRenderingFragmentShadingRateAttachmentInfoKHR);

    void ResolveImage(
        VkImageAspectFlags                 aspectMask,
        const DynamicRenderingAttachments& dynamicRenderingAttachments);

    void LoadOpClearColor(
        const Pal::Rect*          pDeviceGroupRenderArea,
        const VkRenderingInfoKHR* pRenderingInfo);

    void LoadOpClearDepthStencil(
        const Pal::Rect*          pDeviceGroupRenderArea,
        const VkRenderingInfoKHR* pRenderingInfo);

    void GetImageLayout(
        VkImageView        imageView,
        VkImageLayout      imageLayout,
        VkImageAspectFlags aspectMask,
        Pal::SubresRange*  palSubresRange,
        Pal::ImageLayout*  palImageLayout);

    void StoreAttachmentInfo(
        const VkRenderingAttachmentInfoKHR& renderingAttachmentInfo,
        DynamicRenderingAttachments*        pDynamicRenderingAttachement);

    Pal::ImageLayout RPGetAttachmentLayout(
        uint32_t attachment,
        uint32_t plane)
    {
        VK_ASSERT(attachment < m_allGpuState.pRenderPass->GetAttachmentCount());
        VK_ASSERT(attachment < m_renderPassInstance.maxAttachmentCount);
        return m_renderPassInstance.pAttachments[attachment].planeLayout[plane];
    }

    void RPSetAttachmentLayout(
        uint32_t attachment,
        uint32_t plane,
        Pal::ImageLayout layout)
    {
        VK_ASSERT(attachment < m_allGpuState.pRenderPass->GetAttachmentCount());
        VK_ASSERT(attachment < m_renderPassInstance.maxAttachmentCount);
        m_renderPassInstance.pAttachments[attachment].planeLayout[plane] = layout;
    }

    void FillTimestampQueryPool(
        const TimestampQueryPool& timestampQueryPool,
        const uint32_t            firstQuery,
        const uint32_t            queryCount,
        const uint32_t            timestampChunk);

#if VKI_RAY_TRACING
    void ResetAccelerationStructureQueryPool(
        const AccelerationStructureQueryPool& accelerationStructureQueryPool,
        const uint32_t                        firstQuery,
        const uint32_t                        queryCount);
#endif
    void ReleaseResources();

#if VK_ENABLE_DEBUG_BARRIERS
    void DbgCmdBarrier(bool preCmd);
#endif

    template <uint32_t numPalDevices, bool useCompactDescriptor>
    void BindDescriptorSets(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        const VkDescriptorSet*                      pDescriptorSets,
        uint32_t                                    dynamicOffsetCount,
        const uint32_t*                             pDynamicOffsets);

    void BindDescriptorSetsBuffers(
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        const DescriptorBuffers*                    pDescriptorBuffers);

    void SetUserDataPipelineLayout(
        uint32_t                                    firstSet,
        uint32_t                                    setCount,
        const PipelineLayout*                       pLayout,
        const Pal::PipelineBindPoint                palBindPoint,
        const PipelineBindPoint                     apiBindPoint);

    template<uint32_t numPalDevices, bool useCompactDescriptor>
    static VKAPI_ATTR void VKAPI_CALL CmdBindDescriptorSets(
        VkCommandBuffer                             cmdBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    firstSet,
        uint32_t                                    descriptorSetCount,
        const VkDescriptorSet*                      pDescriptorSets,
        uint32_t                                    dynamicOffsetCount,
        const uint32_t*                             pDynamicOffsets);

    template <uint32_t numPalDevices>
    static PFN_vkCmdBindDescriptorSets GetCmdBindDescriptorSetsFunc(const Device* pDevice);

    template <uint32_t numPalDevices>
    VkDescriptorSet InitPushDescriptorSet(
        const DescriptorSetLayout*               pDestSetLayout,
        const PipelineLayout::SetUserDataLayout& setLayoutInfo,
        const size_t                             descriptorSetSize,
        PipelineBindPoint                        bindPoint,
        const uint32_t                           alignmentInDwords);

    template <uint32_t numPalDevices>
    static PFN_vkCmdPushDescriptorSetKHR GetCmdPushDescriptorSetKHRFunc(const Device* pDevice);

    template <size_t imageDescSize,
              size_t samplerDescSize,
              size_t bufferDescSize,
              uint32_t numPalDevices>
    static VKAPI_ATTR void VKAPI_CALL CmdPushDescriptorSetKHR(
        VkCommandBuffer                             commandBuffer,
        VkPipelineBindPoint                         pipelineBindPoint,
        VkPipelineLayout                            layout,
        uint32_t                                    set,
        uint32_t                                    descriptorWriteCount,
        const VkWriteDescriptorSet*                 pDescriptorWrites);

    template <uint32_t numPalDevices>
    static VKAPI_ATTR void VKAPI_CALL CmdPushDescriptorSetWithTemplateKHR(
        VkCommandBuffer                             commandBuffer,
        VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
        VkPipelineLayout                            layout,
        uint32_t                                    set,
        const void*                                 pData);

    bool PalPipelineBindingOwnedBy(
        Pal::PipelineBindPoint palBind,
        PipelineBindPoint apiBind
        ) const
        {
            return m_allGpuState.palToApiPipeline[static_cast<uint32_t>(palBind)] == apiBind;
        }

    static void ConvertPipelineBindPoint(
        VkPipelineBindPoint     pipelineBindPoint,
        Pal::PipelineBindPoint* pPalBindPoint,
        PipelineBindPoint*      pApiBind);

    void PushConstantsIssueWrites(
        const PipelineLayout*  pLayout,
        VkShaderStageFlags     stageFlags,
        uint32_t               startInDwords,
        uint32_t               lengthInDwords,
        const uint32_t* const  pInputValues);

    void WritePushConstants(
        PipelineBindPoint      apiBindPoint,
        Pal::PipelineBindPoint palBindPoint,
        const PipelineLayout*  pLayout,
        uint32_t               startInDwords,
        uint32_t               lengthInDwords,
        const uint32_t* const  pInputValues);

    void InitializeVertexBuffer();
    void ResetVertexBuffer();
    void UpdateVertexBufferStrides(const GraphicsPipeline* pPipeline);

    void UpdateLargestPipelineStackSize(const uint32_t deviceIndex, const uint32_t pipelineStackSize)
    {
        PerGpuState(deviceIndex)->maxPipelineStackSize =
            Util::Max(PerGpuState(deviceIndex)->maxPipelineStackSize, pipelineStackSize);
    }

    void BindAlternatingThreadGroupConstant();

    DynamicVertexInputInternalData* BuildUberFetchShaderInternalData(
        uint32_t                                     vertexBindingDescriptionCount,
        const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
        uint32_t                                     vertexAttributeDescriptionCount,
        const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);

#if VKI_RAY_TRACING
    void BuildAccelerationStructuresPerDevice(
        const uint32_t                                          deviceIndex,
        uint32_t                                                infoCount,
        const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
        const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos,
        const VkDeviceAddress*                                  pIndirectDeviceAddresses,
        const uint32*                                           pIndirectStrides,
        const uint32* const*                                    ppMaxPrimitiveCounts);

    void CopyAccelerationStructurePerDevice(
        const uint32_t                              deviceIndex,
        const VkCopyAccelerationStructureInfoKHR*   pInfo);

    void CopyAccelerationStructureToMemoryPerDevice(
        const uint32_t                                    deviceIndex,
        const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo);

    void WriteAccelerationStructuresPropertiesPerDevice (
        const uint32_t                      deviceIndex,
        uint32_t                            accelerationStructureCount,
        const VkAccelerationStructureKHR*   pAccelerationStructures,
        VkQueryType                         queryType,
        VkQueryPool                         queryPool,
        uint32_t                            firstQuery);

    void CopyMemoryToAccelerationStructurePerDevice(
        const uint32_t                                      deviceIndex,
        const VkCopyMemoryToAccelerationStructureInfoKHR*   pInfo);

    void TraceRaysPerDevice(
        const uint32_t                         deviceIdx,
        const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
        uint32_t                               width,
        uint32_t                               height,
        uint32_t                               depth);

    void TraceRaysIndirectPerDevice(
        const uint32_t                         deviceIdx,
        GpuRt::ExecuteIndirectArgType          indirectArgType,
        const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
        VkDeviceAddress                        indirectDeviceAddress);

    void GetRayTracingDispatchArgs(
        uint32_t                               deviceIdx,
        const RuntimeSettings&                 settings,
        CmdPool*                               pCmdPool,
        const RayTracingPipeline*              pPipeline,
        Pal::gpusize                           constGpuAddr,
        uint32_t                               width,
        uint32_t                               height,
        uint32_t                               depth,
        const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
        const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
        GpuRt::DispatchRaysConstants*          pConstants);

    void BindRayQueryConstants(
        const Pipeline*        pPipeline,
        Pal::PipelineBindPoint bindPoint,
        uint32_t               width,
        uint32_t               height,
        uint32_t               depth,
        Buffer*                pIndirectBuffer,
        VkDeviceSize           indirectOffset);
#endif

    void InsertDebugMarker(
        const char* pLabelName,
        bool        isBegin);

    union CmdBufferFlags
    {
        uint32_t u32All;

        struct
        {
            uint32_t is2ndLvl                            :  1;
            uint32_t isRecording                         :  1;
            uint32_t wasBegun                            :  1;
            uint32_t hasConditionalRendering             :  1;
            uint32_t padVertexBuffers                    :  1;
            uint32_t prefetchCommands                    :  1;
            uint32_t prefetchShaders                     :  1;
            uint32_t disableResetReleaseResources        :  1;
            uint32_t subpassLoadOpClearsBoundAttachments :  1;
            uint32_t preBindDefaultState                 :  1;
            uint32_t useReleaseAcquire                   :  1;
            uint32_t useSplitReleaseAcquire              :  1;
            uint32_t useBackupBuffer : 1;
            uint32_t reserved2                           :  3;
            uint32_t isRenderingSuspended                :  1;
#if VKI_RAY_TRACING
            uint32_t hasRayTracing                       :  1;
#else
            uint32_t reserved4                           :  1;
#endif
            uint32_t reserved                            : 14;
        };
    };

    Device* const                 m_pDevice;
    CmdPool* const                m_pCmdPool;
    uint32_t                      m_queueFamilyIndex;
    uint32_t                      m_backupQueueFamilyIndex;
    Pal::QueueType                m_palQueueType;
    Pal::EngineType               m_palEngineType;
    uint32_t                      m_curDeviceMask;     // Device mask the command buffer is currently set to
    uint32_t                      m_rpDeviceMask;      // Device mask for the render pass instance
    uint32_t                      m_cbBeginDeviceMask; // Device mask this command buffer began with
    const uint32_t                m_numPalDevices;
    VkShaderStageFlags            m_validShaderStageFlags;
    Pal::ICmdBuffer*              m_pPalCmdBuffers[MaxPalDevices];
    Pal::ICmdBuffer*              m_pBackupPalCmdBuffers[MaxPalDevices];
    VirtualStackAllocator*        m_pStackAllocator;

    AllGpuRenderState             m_allGpuState; // Render state tracked during command buffer building

    CmdBufferFlags                m_flags;
    OptimizeCmdbufMode            m_optimizeCmdbufMode;
    uint32_t                      m_asyncComputeQueueMaxWavesPerCu;

    VkResult                      m_recordingResult; // Tracks the result of recording commands to capture OOM errors

    SqttCmdBufferState*           m_pSqttState; // Per-cmdbuf state for handling SQ thread-tracing annotations

    RenderPassInstanceState       m_renderPassInstance;
    TransformFeedbackState*       m_pTransformFeedbackState;

#if VK_ENABLE_DEBUG_BARRIERS
    uint64_t                      m_dbgBarrierPreCmdMask;
    uint64_t                      m_dbgBarrierPostCmdMask;
#endif

    Util::Vector<DynamicDepthStencil, 16, PalAllocator> m_palDepthStencilState;
    Util::Vector<DynamicColorBlend, 16, PalAllocator>   m_palColorBlendState;
    Util::Vector<DynamicMsaa, 16, PalAllocator>         m_palMsaaState;

    UberFetchShaderInternalDataMap m_uberFetchShaderInternalDataMap;// Uber fetch shader internal data cache
    void* m_pUberFetchShaderTempBuffer;

    uint32                        m_vbWatermark;  // tracks how many vb entries need to be reset
    DebugPrintf                   m_debugPrintf;
    bool                          m_reverseThreadGroupState;
#if VKI_RAY_TRACING
    Util::Vector<InternalMemory*, 16, PalAllocator> m_rayTracingIndirectList; // Ray-tracing indirect memory
#endif
};

// =====================================================================================================================

// =====================================================================================================================
void CmdBuffer::PalCmdBindMsaaState(
    Pal::ICmdBuffer*       pPalCmdBuf,
    uint32_t               deviceIdx,
    const Pal::IMsaaState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_curDeviceMask) != 0);

    if (pState != PerGpuState(deviceIdx)->pMsaaState)
    {
        pPalCmdBuf->CmdBindMsaaState(pState);

        PerGpuState(deviceIdx)->pMsaaState = pState;
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindColorBlendState(
    Pal::ICmdBuffer*             pPalCmdBuf,
    uint32_t                     deviceIdx,
    const Pal::IColorBlendState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_curDeviceMask) != 0);

    if (pState != PerGpuState(deviceIdx)->pColorBlendState)
    {
        pPalCmdBuf->CmdBindColorBlendState(pState);

        PerGpuState(deviceIdx)->pColorBlendState = pState;
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindDepthStencilState(
    Pal::ICmdBuffer*               pPalCmdBuf,
    uint32_t                       deviceIdx,
    const Pal::IDepthStencilState* pState)
{
    VK_ASSERT(((1UL << deviceIdx) & m_curDeviceMask) != 0);

    if (pState != PerGpuState(deviceIdx)->pDepthStencilState)
    {
        pPalCmdBuf->CmdBindDepthStencilState(pState);

        PerGpuState(deviceIdx)->pDepthStencilState = pState;
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
    for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
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
    for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        pDestEvents[(deviceIdx * stride) + index] = pSrcEvents->PalEvent(deviceIdx);
    }
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

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCount(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCount(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectEXT(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectCountEXT(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
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

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer                             commandBuffer,
    VkSubpassContents                           contents);

VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo,
    const VkSubpassEndInfo*                     pSubpassEndInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassEndInfo*                     pSubpassEndInfo);

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

VKAPI_ATTR void VKAPI_CALL vkCmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets);

VKAPI_ATTR void VKAPI_CALL vkCmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginQueryIndexedEXT(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    VkQueryControlFlags                         flags,
    uint32_t                                    index);

VKAPI_ATTR void VKAPI_CALL vkCmdEndQueryIndexedEXT(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    uint32_t                                    index);

VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectByteCountEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    instanceCount,
    uint32_t                                    firstInstance,
    VkBuffer                                    counterBuffer,
    VkDeviceSize                                counterBufferOffset,
    uint32_t                                    counterOffset,
    uint32_t                                    vertexStride);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pLabelInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pLabelInfo);

#if VKI_RAY_TRACING
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth);

VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer                                         commandBuffer,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos);

VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer                                    commandBuffer,
    uint32                                             infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkDeviceAddress*                             pIndirectDeviceAddresses,
    const uint32*                                      pIndirectStrides,
    const uint32* const*                               ppMaxPrimitiveCounts);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureKHR(
    VkCommandBuffer                                    commandBuffer,
    const VkCopyAccelerationStructureInfoKHR*          pInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer                                    commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR*  pInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer                                    commandBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR*  pInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdSetRayTracingPipelineStackSizeKHR(
    VkCommandBuffer                                    commandBuffer,
    uint32_t                                           pipelineStackSize);

VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirectKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    VkDeviceAddress                             indirectDeviceAddress);

VKAPI_ATTR void VKAPI_CALL vkCmdWriteAccelerationStructuresPropertiesKHR(
    VkCommandBuffer                                    commandBuffer,
    uint32_t                                           accelerationStructureCount,
    const VkAccelerationStructureKHR*                  pAccelerationStructures,
    VkQueryType                                        queryType,
    VkQueryPool                                        queryPool,
    uint32_t                                           firstQuery);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer                                   commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirect2KHR(
    VkCommandBuffer                             commandBuffer,
    VkDeviceAddress                             indirectDeviceAddress);
#endif

VKAPI_ATTR void VKAPI_CALL vkCmdSetFragmentShadingRateKHR(
    VkCommandBuffer                          commandBuffer,
    const VkExtent2D*                        pFragmentSize,
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2]);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginConditionalRenderingEXT(
    VkCommandBuffer                           commandBuffer,
    const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin);

VKAPI_ATTR void VKAPI_CALL vkCmdEndConditionalRenderingEXT(
    VkCommandBuffer                           commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent2(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    const VkDependencyInfoKHR*                  pDependencyInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent2(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags2KHR                    stageMask);

VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents2(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    const VkDependencyInfoKHR*                  pDependencyInfos);

VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(
    VkCommandBuffer                             commandBuffer,
    const VkDependencyInfoKHR*                  pDependencyInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp2(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags2KHR                    stage,
    VkQueryPool                                 queryPool,
    uint32_t                                    query);

VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarker2AMD(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags2KHR                    stage,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    uint32_t                                    marker);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRendering(
    VkCommandBuffer           commandBuffer,
    const VkRenderingInfoKHR* pRenderingInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdEndRendering(
    VkCommandBuffer           commandBuffer);

VKAPI_ATTR void VKAPI_CALL vkCmdSetCullMode(
    VkCommandBuffer                             commandBuffer,
    VkCullModeFlags                             cullMode);

VKAPI_ATTR void VKAPI_CALL vkCmdSetFrontFace(
    VkCommandBuffer                             commandBuffer,
    VkFrontFace                                 frontFace);

VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveTopology(
    VkCommandBuffer                             commandBuffer,
    VkPrimitiveTopology                         primitiveTopology);

VKAPI_ATTR void VKAPI_CALL vkCmdSetViewportWithCount(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports);

VKAPI_ATTR void VKAPI_CALL vkCmdSetScissorWithCount(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors);

VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers2(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes,
    const VkDeviceSize*                         pStrides);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthTestEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthWriteEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthWriteEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthCompareOp(
    VkCommandBuffer                             commandBuffer,
    VkCompareOp                                 depthCompareOp);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBoundsTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBoundsTestEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    stencilTestEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilOp(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    VkStencilOp                                 failOp,
    VkStencilOp                                 passOp,
    VkStencilOp                                 depthFailOp,
    VkCompareOp                                 compareOp);

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    bufferCount,
    const VkDescriptorBufferBindingInfoEXT*     pBindingInfos);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDescriptorBufferOffsetsEXT(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    setCount,
    const uint32_t*                             pBufferIndices,
    const VkDeviceSize*                         pOffsets);

VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorBufferEmbeddedSamplersEXT(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set);

VKAPI_ATTR void VKAPI_CALL vkCmdSetColorWriteEnableEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    attachmentCount,
    const VkBool32*                             pColorWriteEnables);

VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizerDiscardEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    rasterizerDiscardEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveRestartEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    primitiveRestartEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBiasEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBiasEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLogicOpEXT(
    VkCommandBuffer                             commandBuffer,
    VkLogicOp                                   logicOp);

VKAPI_ATTR void VKAPI_CALL vkCmdSetPatchControlPointsEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    patchControlPoints);

VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage2(
    VkCommandBuffer                             commandBuffer,
    const VkBlitImageInfo2KHR*                  pBlitImageInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyBufferInfo2KHR*                 pCopyBufferInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyBufferToImageInfo2KHR*          pCopyBufferToImageInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyImageInfo2KHR*                  pCopyImageInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyImageToBufferInfo2KHR*          pCopyImageToBufferInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage2(
    VkCommandBuffer                             commandBuffer,
    const VkResolveImageInfo2KHR*               pResolveImageInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites);

VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer                             commandBuffer,
    VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    const void*                                 pData);

VKAPI_ATTR void VKAPI_CALL vkCmdSetTessellationDomainOriginEXT(
    VkCommandBuffer                     commandBuffer,
    VkTessellationDomainOrigin          domainOrigin);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClampEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            depthClampEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetPolygonModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkPolygonMode                       polygonMode);

VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizationSamplesEXT(
    VkCommandBuffer                     commandBuffer,
    VkSampleCountFlagBits               rasterizationSamples);

VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleMaskEXT(
    VkCommandBuffer                     commandBuffer,
    VkSampleCountFlagBits               samples,
    const VkSampleMask*                 pSampleMask);

VKAPI_ATTR void VKAPI_CALL vkCmdSetAlphaToCoverageEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            alphaToCoverageEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetAlphaToOneEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            alphaToOneEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLogicOpEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            logicOpEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendEnableEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkBool32*                     pColorBlendEnables);

VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendEquationEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkColorBlendEquationEXT*      pColorBlendEquations);

VKAPI_ATTR void VKAPI_CALL vkCmdSetColorWriteMaskEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const  VkColorComponentFlags*       pColorWriteMasks);

VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizationStreamEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            rasterizationStream);

VKAPI_ATTR void VKAPI_CALL vkCmdSetConservativeRasterizationModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkConservativeRasterizationModeEXT  conservativeRasterizationMode);

VKAPI_ATTR void VKAPI_CALL vkCmdSetExtraPrimitiveOverestimationSizeEXT(
    VkCommandBuffer                     commandBuffer,
    float                               extraPrimitiveOverestimationSize);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClipEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            depthClipEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleLocationsEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            sampleLocationsEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendAdvancedEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkColorBlendAdvancedEXT*      pColorBlendAdvanced);

VKAPI_ATTR void VKAPI_CALL vkCmdSetProvokingVertexModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkProvokingVertexModeEXT            provokingVertexMode);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLineRasterizationModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkLineRasterizationModeEXT          lineRasterizationMode);

VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            stippledLineEnable);

VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClipNegativeOneToOneEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            negativeOneToOne);

VKAPI_ATTR void VKAPI_CALL vkCmdSetVertexInputEXT(
    VkCommandBuffer                              commandBuffer,
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions);

} // namespace entry

} // namespace vk

#endif /* __VK_CMDBUFFER_H__ */
