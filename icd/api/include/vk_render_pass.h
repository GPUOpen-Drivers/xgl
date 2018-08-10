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

#ifndef __VK_RENDER_PASS_H__
#define __VK_RENDER_PASS_H__

#pragma once

#include <limits.h>

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_object.h"
#include "include/vk_framebuffer.h"

#include "renderpass/renderpass_types.h"
#include "renderpass/renderpass_builder.h"
#include "renderpass/renderpass_logger.h"

#include "palCmdBuffer.h"
#include "palVector.h"

namespace vk
{

class Device;
class CmdBuffer;
class Framebuffer;
class GpuEvents;
class RenderPassCmdList;

struct RenderPassExtCreateInfo
{
    RenderPassExtCreateInfo()
        :
        pMultiviewCreateInfo            (nullptr)
    {
    }

    const VkRenderPassMultiviewCreateInfo*                          pMultiviewCreateInfo;
};

struct AttachmentReference
{
    AttachmentReference();

    void Init(const VkAttachmentReference&      attachRef);
    void Init(const VkAttachmentReference2KHR&  attachRef);

    uint32_t              attachment;
    VkImageLayout         layout;
    VkImageAspectFlags    aspectMask;
};

struct AttachmentDescription
{
    AttachmentDescription();

    void Init(const VkAttachmentDescription&     attachDesc);
    void Init(const VkAttachmentDescription2KHR& attachDesc);

    VkAttachmentDescriptionFlags    flags;
    VkFormat                        format;
    VkSampleCountFlagBits           samples;
    VkAttachmentLoadOp              loadOp;
    VkAttachmentStoreOp             storeOp;
    VkAttachmentLoadOp              stencilLoadOp;
    VkAttachmentStoreOp             stencilStoreOp;
    VkImageLayout                   initialLayout;
    VkImageLayout                   finalLayout;
};

struct SubpassSampleCount
{
    SubpassSampleCount()
        :
        colorCount(0),
        depthCount(0)
    {
    }

    uint32_t colorCount;
    uint32_t depthCount;
};

struct SubpassDescription
{
    SubpassDescription();

    void Init(
        uint32_t                        subpassIndex,
        const VkSubpassDescription&     subpassDesc,
        const RenderPassExtCreateInfo&  renderPassExt,
        const AttachmentDescription*    pAttachments,
        uint32_t                        attachmentCount,
        void*                           pMemoryPtr,
        size_t                          memorySize);

    void Init(
        uint32_t                        subpassIndex,
        const VkSubpassDescription2KHR& subpassDesc,
        const RenderPassExtCreateInfo&  renderPassExt,
        const AttachmentDescription*    pAttachments,
        uint32_t                        attachmentCount,
        void*                           pMemoryPtr,
        size_t                          memorySize);

    VkSubpassDescriptionFlags   flags;
    VkPipelineBindPoint         pipelineBindPoint;
    uint32_t                    viewMask;
    uint32_t                    inputAttachmentCount;
    AttachmentReference*        pInputAttachments;
    uint32_t                    colorAttachmentCount;
    AttachmentReference*        pColorAttachments;
    AttachmentReference*        pResolveAttachments;
    AttachmentReference         depthStencilAttachment;
    uint32_t                    preserveAttachmentCount;
    uint32_t*                   pPreserveAttachments;

    SubpassSampleCount          subpassSampleCount;
};

struct SubpassDependency
{
    SubpassDependency();

    void Init(
        uint32_t                        subpassDepIndex,
        const VkSubpassDependency&      subpassDep,
        const RenderPassExtCreateInfo&  renderPassExt);

    void Init(
        uint32_t                        subpassDepIndex,
        const VkSubpassDependency2KHR&  subpassDep,
        const RenderPassExtCreateInfo&  renderPassExt);

    uint32_t                srcSubpass;
    uint32_t                dstSubpass;
    VkPipelineStageFlags    srcStageMask;
    VkPipelineStageFlags    dstStageMask;
    VkAccessFlags           srcAccessMask;
    VkAccessFlags           dstAccessMask;
    VkDependencyFlags       dependencyFlags;
    int32_t                 viewOffset;
};

struct RenderPassCreateInfo
{
    RenderPassCreateInfo();

    void Init(
        const VkRenderPassCreateInfo*       pCreateInfo,
        const RenderPassExtCreateInfo&      renderPassExt,
        void*                               pMemoryPtr,
        size_t                              memorySize);

    void Init(
        const VkRenderPassCreateInfo2KHR*   pCreateInfo,
        const RenderPassExtCreateInfo&      renderPassExt,
        void*                               pMemoryPtr,
        size_t                              memorySize);

    VkRenderPassCreateFlags  flags;
    uint32_t                 attachmentCount;
    AttachmentDescription*   pAttachments;
    uint32_t                 subpassCount;
    SubpassDescription*      pSubpasses;
    uint32_t                 dependencyCount;
    SubpassDependency*       pDependencies;
    uint32_t                 correlatedViewMaskCount;
    uint32_t*                pCorrelatedViewMasks;
    uint64_t                 hash;
};

// =====================================================================================================================
// Implementation of a Vulkan render pass (VkRenderPass)
class RenderPass : public NonDispatchable<VkRenderPass, RenderPass>
{
public:

    static VkResult Create(
        Device*                             pDevice,
        const VkRenderPassCreateInfo*       pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkRenderPass*                       pRenderPass);

    static VkResult Create(
        Device*                             pDevice,
        const VkRenderPassCreateInfo2KHR*   pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkRenderPass*                       pRenderPass);

    RenderPass(
        const RenderPassCreateInfo*     pCreateInfo,
        const RenderPassExecuteInfo*    pExecuteInfo);

    VkResult Destroy(
        const Device*                 pDevice,
        const VkAllocationCallbacks*  pAllocator);

    VkFormat GetColorAttachmentFormat(uint32_t subPassIndex, uint32_t colorTarget) const;
    VkFormat GetDepthStencilAttachmentFormat(uint32_t subPassIndex) const;

    uint32_t GetColorAttachmentSamples(uint32_t subPassIndex, uint32_t colorTarget) const;
    uint32_t GetDepthStencilAttachmentSamples(uint32_t subPassIndex) const;

    uint32_t GetSubpassColorReferenceCount(uint32_t subPassIndex) const;
    VK_INLINE uint32_t GetAttachmentCount() const { return m_createInfo.attachmentCount; }
    const AttachmentDescription& GetAttachmentDesc(uint32_t attachmentIndex) const;
    const AttachmentReference& GetSubpassColorReference(uint32_t subpass, uint32_t index) const;
    const AttachmentReference& GetSubpassDepthStencilReference(uint32_t subpass) const;

    VK_INLINE const uint32_t GetSubpassMaxSampleCount(uint32_t subpass) const
    {
        return Util::Max(m_createInfo.pSubpasses[subpass].subpassSampleCount.colorCount,
                         m_createInfo.pSubpasses[subpass].subpassSampleCount.depthCount);
    }

    VK_INLINE const uint32_t GetSubpassColorSampleCount(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].subpassSampleCount.colorCount; }

    VK_INLINE const uint32_t GetSubpassDepthSampleCount(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].subpassSampleCount.depthCount; }

    VK_INLINE const RenderPassExecuteInfo* GetExecuteInfo() const
        { return m_pExecuteInfo; }

    VK_INLINE uint64_t GetHash() const
        { return m_createInfo.hash; }

    VK_INLINE uint32_t GetSubpassCount() const
        { return m_createInfo.subpassCount; }

    VK_INLINE uint32_t GetViewMask(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].viewMask; }

    VK_INLINE uint32_t GetActiveViewsBitMask() const
    {
        uint32_t activeViewsBitMask = 0;

        // View is considered active when it is used in any subpass defined by RenderPass.
        for (uint32_t subpass = 0; subpass < GetSubpassCount(); ++subpass)
        {
            activeViewsBitMask |= GetViewMask(subpass);
        }

        // ActiveViewsBitMask can be understood as RenderPass ViewMask.
        return activeViewsBitMask;
    }

    VK_INLINE bool IsMultiviewEnabled() const
    {
        // When a subpass uses a non-zero view mask,
        // multiview functionality is considered to be enabled.
        //
        // Multiview is all-or-nothing for a render pass - that is,
        // either all subpasses must have a non-zero view mask
        // (though some subpasses may have only one view) or all must be zero.
        return GetViewMask(0) != 0;
    }

protected:

    const RenderPassCreateInfo     m_createInfo;
    const RenderPassExecuteInfo*   m_pExecuteInfo;
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(
    VkDevice                     device,
    VkRenderPass                 renderPass,
    const VkAllocationCallbacks* pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_RENDER_PASS_H__ */
