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

#ifndef __VK_RENDER_PASS_H__
#define __VK_RENDER_PASS_H__

#pragma once

#include <limits.h>

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_framebuffer.h"

#include "renderpass/renderpass_types.h"
#include "renderpass/renderpass_builder.h"

#include "palCmdBuffer.h"
#include "palVector.h"

namespace vk
{

class Device;
class CmdBuffer;
class Framebuffer;
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

    void Init(const VkAttachmentReference&  attachRef);
    void Init(const VkAttachmentReference2& attachRef);

    uint32_t              attachment;
    VkImageLayout         layout;
    VkImageLayout         stencilLayout;
    VkImageAspectFlags    aspectMask;
};

struct AttachmentDescription
{
    AttachmentDescription();

    void Init(const VkAttachmentDescription&  attachDesc);
    void Init(const VkAttachmentDescription2& attachDesc);

    VkAttachmentDescriptionFlags    flags;
    VkFormat                        format;
    VkSampleCountFlagBits           samples;
    VkAttachmentLoadOp              loadOp;
    VkAttachmentStoreOp             storeOp;
    VkAttachmentLoadOp              stencilLoadOp;
    VkAttachmentStoreOp             stencilStoreOp;
    VkImageLayout                   initialLayout;
    VkImageLayout                   finalLayout;
    VkImageLayout                   stencilInitialLayout;
    VkImageLayout                   stencilFinalLayout;
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
        const VkSubpassDescription2&    subpassDesc,
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

    VkResolveModeFlagBits       depthResolveMode;
    VkResolveModeFlagBits       stencilResolveMode;
    AttachmentReference         depthStencilResolveAttachment;

    AttachmentReference         fragmentShadingRateAttachment;

    SubpassSampleCount          subpassSampleCount;
    uint64_t                    hash;
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
        const VkSubpassDependency2&     subpassDep,
        const RenderPassExtCreateInfo&  renderPassExt);

    uint32_t                srcSubpass;
    uint32_t                dstSubpass;
    PipelineStageFlags      srcStageMask;
    PipelineStageFlags      dstStageMask;
    AccessFlags             srcAccessMask;
    AccessFlags             dstAccessMask;
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
        const VkRenderPassCreateInfo2*      pCreateInfo,
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
    bool                     needForceLateZ;
    uint64_t                 hash;
};

// =====================================================================================================================
// Implementation of a Vulkan render pass (VkRenderPass)
class RenderPass final : public NonDispatchable<VkRenderPass, RenderPass>
{
public:

    static VkResult Create(
        Device*                             pDevice,
        const VkRenderPassCreateInfo*       pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkRenderPass*                       pRenderPass);

    static VkResult Create(
        Device*                             pDevice,
        const VkRenderPassCreateInfo2*      pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkRenderPass*                       pRenderPass);

    RenderPass(
        const RenderPassCreateInfo*     pCreateInfo,
        const RenderPassExecuteInfo*    pExecuteInfo);

    VkResult Destroy(
        Device*                       pDevice,
        const VkAllocationCallbacks*  pAllocator);

    VkFormat GetColorAttachmentFormat(uint32_t subPassIndex, uint32_t colorTarget) const;
    VkFormat GetDepthStencilAttachmentFormat(uint32_t subPassIndex) const;

    uint32_t GetColorAttachmentSamples(uint32_t subPassIndex, uint32_t colorTarget) const;
    uint32_t GetDepthStencilAttachmentSamples(uint32_t subPassIndex) const;
    VkResolveModeFlagBits GetDepthResolveMode(uint32_t subpass) const
    {
        return m_createInfo.pSubpasses[subpass].depthResolveMode;
    }

    VkResolveModeFlagBits GetStencilResolveMode(uint32_t subpass) const
    {
        return m_createInfo.pSubpasses[subpass].stencilResolveMode;
    }

    VkImageAspectFlags GetResolveDepthStecilAspect(uint32_t subpass) const
    {
        return m_createInfo.pSubpasses[subpass].depthStencilResolveAttachment.aspectMask;
    }

    uint32_t GetSubpassColorReferenceCount(uint32_t subPassIndex) const;
    uint32_t GetAttachmentCount() const { return m_createInfo.attachmentCount; }
    const AttachmentDescription& GetAttachmentDesc(uint32_t attachmentIndex) const;
    const AttachmentReference& GetSubpassColorReference(uint32_t subpass, uint32_t index) const;
    const AttachmentReference& GetSubpassDepthStencilReference(uint32_t subpass) const;

    uint32_t GetSubpassMaxSampleCount(uint32_t subpass) const
    {
        return Util::Max(m_createInfo.pSubpasses[subpass].subpassSampleCount.colorCount,
                         m_createInfo.pSubpasses[subpass].subpassSampleCount.depthCount);
    }

    uint32_t GetSubpassColorSampleCount(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].subpassSampleCount.colorCount; }

    uint32_t GetSubpassDepthSampleCount(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].subpassSampleCount.depthCount; }

    const RenderPassExecuteInfo* GetExecuteInfo() const
        { return m_pExecuteInfo; }

    uint64_t GetHash() const
        { return m_createInfo.hash; }

    uint64_t GetSubpassHash(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].hash; }

    uint32_t GetSubpassCount() const
        { return m_createInfo.subpassCount; }

    uint32_t GetViewMask(uint32_t subpass) const
        { return m_createInfo.pSubpasses[subpass].viewMask; }

    uint32_t GetActiveViewsBitMask() const
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

    bool IsMultiviewEnabled() const
    {
        // When a subpass uses a non-zero view mask,
        // multiview functionality is considered to be enabled.
        //
        // Multiview is all-or-nothing for a render pass - that is,
        // either all subpasses must have a non-zero view mask
        // (though some subpasses may have only one view) or all must be zero.
        return GetViewMask(0) != 0;
    }

    bool IsForceLateZNeeded() const
    {
        return m_createInfo.needForceLateZ;
    }

protected:
    const RenderPassCreateInfo     m_createInfo;
    const RenderPassExecuteInfo*   m_pExecuteInfo;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(RenderPass);
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
