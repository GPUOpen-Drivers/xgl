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

#include "include/vk_instance.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_formats.h"
#include "include/vk_framebuffer.h"
#include "include/vk_image_view.h"
#include "include/vk_render_pass.h"

#include "renderpass/renderpass_builder.h"

#include "palListImpl.h"
#include "palVectorImpl.h"

#define RPBUILD_NEW_ARRAY(_classname, _count) PAL_NEW_ARRAY(_classname, _count, m_pArena, Util::AllocInternalTemp)
#define RPBUILD_NEW(_classname) PAL_NEW(_classname, m_pArena, Util::AllocInternalTemp)

namespace vk
{

constexpr VkPipelineStageFlags2 AllShaderStages =
    VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT                  |
    VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT    |
    VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
    VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT                |
    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

// =====================================================================================================================
RenderPassBuilder::RenderPassBuilder(
    Device*                  pDevice,
    utils::TempMemArena*     pArena)
    :
    m_pInfo(nullptr),
    m_pDevice(pDevice),
    m_pArena(pArena),
    m_attachmentCount(0),
    m_pAttachments(nullptr),
    m_subpassCount(0),
    m_pSubpasses(nullptr),
    m_endState(pArena)
{

}

// =====================================================================================================================
RenderPassBuilder::~RenderPassBuilder()
{
    // Invoke the destructors on the attachment and subpass arrays.  No need to explicitly free memory because they
    // are allocated from the temp memory arena.

    if (m_pAttachments != nullptr)
    {
        for (uint32_t i = 0; i < m_attachmentCount; ++i)
        {
            Util::Destructor(&m_pAttachments[i]);
        }
    }

    if (m_pSubpasses != nullptr)
    {
        for (uint32_t i = 0; i < m_subpassCount; ++i)
        {
            Util::Destructor(&m_pSubpasses[i]);
        }
    }
}

// =====================================================================================================================
// Initializes state arrays for building a render pass and precomputes some initial derived information.
Pal::Result RenderPassBuilder::BuildInitialState()
{
    Pal::Result result = Pal::Result::Success;

    m_attachmentCount = m_pInfo->attachmentCount;
    m_subpassCount = m_pInfo->subpassCount;

    // Initialize attachment state
    if ((m_pInfo->attachmentCount > 0) && (result == Pal::Result::Success))
    {
        void* pStorage = m_pArena->Alloc(sizeof(AttachmentState) * m_attachmentCount);

        if (pStorage != nullptr)
        {
            m_pAttachments = static_cast<AttachmentState*>(pStorage);

            for (uint32_t i = 0; i < m_attachmentCount; ++i)
            {
                VK_PLACEMENT_NEW(&m_pAttachments[i]) AttachmentState(&m_pInfo->pAttachments[i]);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    // Initialize subpass state
    if ((m_pInfo->subpassCount > 0) && (result == Pal::Result::Success))
    {
        void* pStorage = m_pArena->Alloc(sizeof(SubpassState) * m_subpassCount);

        if (pStorage != nullptr)
        {
            m_pSubpasses = static_cast<SubpassState*>(pStorage);

            for (uint32_t i = 0; i < m_subpassCount; ++i)
            {
                VK_PLACEMENT_NEW(&m_pSubpasses[i]) SubpassState(&m_pInfo->pSubpasses[i], m_pArena);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    if (result == Pal::Result::Success)
    {
        // Find first and last subpass indices that reference each attachment
        for (uint32_t subpass = 0; subpass < m_subpassCount; ++subpass)
        {
            for (uint32_t attachment = 0; attachment < m_attachmentCount; ++attachment)
            {
                // When calculating first use, ignore preserve attachments because this subpass will not use it,
                // and it should be loaded by the first subpass that actually does.
                if ((GetSubpassReferenceMask(subpass, attachment) & ~AttachRefPreserve) != 0)
                {
                    if (m_pAttachments[attachment].firstUseSubpass == VK_SUBPASS_EXTERNAL)
                    {
                        m_pAttachments[attachment].firstUseSubpass = subpass;

                        m_pSubpasses[subpass].flags.hasFirstUseAttachments = true;
                    }

                    m_pAttachments[attachment].finalUseSubpass = subpass;
                }
            }
        }

        // Flag which subpasses contain final-use attachment references
        for (uint32_t attachment = 0; attachment < m_attachmentCount; ++attachment)
        {
            if (m_pAttachments[attachment].finalUseSubpass != VK_SUBPASS_EXTERNAL)
            {
                m_pSubpasses[m_pAttachments[attachment].finalUseSubpass].flags.hasFinalUseAttachments |=
                    ((m_pAttachments[attachment].prevReferenceLayout.layout       !=
                     m_pAttachments[attachment].pDesc->finalLayout)                     ||
                    (m_pAttachments[attachment].prevReferenceStencilLayout.layout !=
                     m_pAttachments[attachment].pDesc->stencilFinalLayout));
            }
        }

        // Sort which subpasses have incoming/outgoing application-provided VkSubpassDependencies.  Spec rules dictate
        // that missing ones are implicitly added (although we don't currently do anything with these)
        for (uint32_t depIdx = 0; depIdx < m_pInfo->dependencyCount; ++depIdx)
        {
            const SubpassDependency& dep = m_pInfo->pDependencies[depIdx];

            if ((dep.srcSubpass == VK_SUBPASS_EXTERNAL) && (dep.dstSubpass != VK_SUBPASS_EXTERNAL))
            {
                m_pSubpasses[dep.dstSubpass].flags.hasExternalIncoming = true;
            }

            if ((dep.dstSubpass == VK_SUBPASS_EXTERNAL) && (dep.srcSubpass != VK_SUBPASS_EXTERNAL))
            {
                m_pSubpasses[dep.srcSubpass].flags.hasExternalOutgoing = true;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function returns a mask of AttachRef* for a particular attachment within a particular subpass.  An AttachRef*
// flag is set if the given attachment is used in that way within the given subpass.
uint32_t RenderPassBuilder::GetSubpassReferenceMask(
    uint32_t subpass,
    uint32_t attachment
    ) const
{
    uint32_t refMask = 0;

    if (subpass == VK_SUBPASS_EXTERNAL)
    {
        return 0;
    }

    const SubpassDescription& desc = *m_pSubpasses[subpass].pDesc;

    if ((desc.colorAttachmentCount > 0) && (desc.pColorAttachments != nullptr))
    {
        for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i)
        {
            if (desc.pColorAttachments[i].attachment == attachment)
            {
                refMask |= AttachRefColor;

                if ((desc.pResolveAttachments != nullptr) &&
                    (desc.pResolveAttachments[i].attachment != VK_ATTACHMENT_UNUSED))
                {
                    refMask |= AttachRefResolveSrc;
                }
            }
        }
    }

    if ((desc.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) &&
        (desc.depthStencilAttachment.attachment == attachment))
    {
        refMask |= AttachRefDepthStencil;

        if (m_pInfo->pSubpasses[subpass].depthStencilResolveAttachment.attachment != VK_ATTACHMENT_UNUSED)
        {
            refMask |= AttachRefResolveSrc;
        }
    }

    if ((desc.inputAttachmentCount > 0) && (desc.pInputAttachments != nullptr))
    {
        for (uint32_t i = 0; i < desc.inputAttachmentCount; ++i)
        {
            if (desc.pInputAttachments[i].attachment == attachment)
            {
                refMask |= AttachRefInput;
            }
        }
    }

    if ((desc.preserveAttachmentCount > 0) && (desc.pPreserveAttachments != nullptr))
    {
        for (uint32_t i = 0; i < desc.preserveAttachmentCount; ++i)
        {
            if (desc.pPreserveAttachments[i] == attachment)
            {
                refMask |= AttachRefPreserve;
            }
        }
    }

    if ((desc.colorAttachmentCount > 0) && (desc.pResolveAttachments != nullptr))
    {
        for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i)
        {
            if (desc.pResolveAttachments[i].attachment == attachment)
            {
                refMask |= AttachRefResolveDst;
            }
        }
    }

    if ((desc.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) &&
        (desc.depthStencilResolveAttachment.attachment != VK_ATTACHMENT_UNUSED) &&
        (desc.depthStencilResolveAttachment.attachment == attachment))
    {
        refMask |= AttachRefResolveDst;
    }

    // If VRS is used set the Fragment Ref
    if (m_pInfo->pSubpasses[subpass].fragmentShadingRateAttachment.attachment == attachment)
    {
        refMask |= AttachRefFragShading;
    }

    return refMask;
}

// =====================================================================================================================
RenderPassBuilder::AttachmentState::AttachmentState(
    const AttachmentDescription* pDesc)
    :
    pDesc(pDesc),
    firstUseSubpass(VK_SUBPASS_EXTERNAL),
    finalUseSubpass(VK_SUBPASS_EXTERNAL),
    prevReferenceSubpass(VK_SUBPASS_EXTERNAL),
    accumulatedRefMask(0),
    loaded(false),
    resolvesInFlight(false)
{
    prevReferenceLayout.layout            = pDesc->initialLayout;
    prevReferenceLayout.extraUsage        = 0;
    prevReferenceStencilLayout.layout     = pDesc->stencilInitialLayout;
    prevReferenceStencilLayout.extraUsage = 0;
}

// =====================================================================================================================
// Builds a render pass execute state from its create info.
VkResult RenderPassBuilder::Build(
    const RenderPassCreateInfo*   pRenderPassInfo,
    const VkAllocationCallbacks*  pAllocator,
    RenderPassExecuteInfo**       ppResult)
{
    m_pInfo    = pRenderPassInfo;

    Pal::Result result = BuildInitialState();

    for (uint32_t subpass = 0; (subpass < m_subpassCount) && (result == Pal::Result::Success); ++subpass)
    {
        result = BuildSubpass(subpass);
    }

    if (result == Pal::Result::Success)
    {
        result = BuildEndState();
    }

    if (result == Pal::Result::Success)
    {
        result = Finalize(pAllocator, ppResult);
    }

    if (result == Pal::Result::Success)
    {
        Cleanup();
    }

    return PalToVkResult(result);
}

// =====================================================================================================================
void RenderPassBuilder::Cleanup()
{

}

// =====================================================================================================================
// Builds the execute state for a particular subpass.
Pal::Result RenderPassBuilder::BuildSubpass(
    uint32_t subpass)
{
    SubpassState* pSubpass = &m_pSubpasses[subpass];

    // Handle dependencies with dstSubpass = this subpass.
    Pal::Result result = BuildSubpassDependencies(subpass, &pSubpass->syncTop);

    // Handle any "implicit" dependencies that are not represented by VkSubpassDependencies but are still required
    // internally.
    if (result == Pal::Result::Success)
    {
        result = BuildImplicitDependencies(subpass, &pSubpass->syncTop);
    }

    // Handle the various kinds of attachment references.  These will call a function to trigger automatic layout
    // transitions also.
    const SubpassDescription& subpassDesc = *pSubpass->pDesc;

    if (result == Pal::Result::Success)
    {
        result = BuildColorAttachmentReferences(subpass, subpassDesc);
    }

    if (result == Pal::Result::Success)
    {
        result = BuildFragmentShadingRateAttachmentReferences(subpass);
    }

    if (result == Pal::Result::Success)
    {
        result = BuildDepthStencilAttachmentReferences(subpass, subpassDesc);
    }

    if (result == Pal::Result::Success)
    {
        result = BuildInputAttachmentReferences(subpass, subpassDesc);
    }

    if (result == Pal::Result::Success)
    {
        result = BuildResolveAttachmentReferences(subpass);
    }

    // If we are clearing more than one color target, then we won't auto-sync (it ends up being slower and causing
    // back-to-back syncs under the current implementation).  This means we need to manually pre-sync also.
    if (pSubpass->colorClears.NumElements() > 1)
    {
        pSubpass->syncTop.barrier.flags.preColorClearSync = 1;
    }

    if (pSubpass->dsClears.NumElements() > 1)
    {
        pSubpass->syncTop.barrier.flags.preDsClearSync = 1;
    }

    // Pre-calculate a master flag for whether this subpass's sync points are active based on what was added to them.
    PostProcessSyncPoint(&pSubpass->syncTop);
    PostProcessSyncPoint(&pSubpass->syncPreResolve);
    PostProcessSyncPoint(&pSubpass->syncBottom);

    return result;
}

// =====================================================================================================================
// Handle the load ops (mainly clears) for attachments.  These calls are triggered from the many per-reference functions
// originating from BuildSubpass(), via TrackAttachmentUsage().
Pal::Result RenderPassBuilder::BuildLoadOps(
    uint32_t subpass,
    uint32_t attachment)
{
    VK_ASSERT(subpass < m_subpassCount);

    Pal::Result result           = Pal::Result::Success;
    SubpassState* pSubpass       = &m_pSubpasses[subpass];
    AttachmentState* pAttachment = &m_pAttachments[attachment];

    VK_ASSERT(subpass == pAttachment->firstUseSubpass);
    VK_ASSERT(pAttachment->loaded == false);

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    bool isOptional = false;

    // Set a flag indicating this attachment has been already loaded once.
    pAttachment->loaded = true;

    // Trigger load op clears if needed on first use.  These clears run auto-synced
    // (see Pal::ICmdBuffer::CmdClear[Color|DepthStencil]Image flags) which means that we do not have to
    // explicitly pre- or post-clear synchronize them using sync points.
    VkImageAspectFlags clearAspect = 0;

    if (Formats::IsColorFormat(pAttachment->pDesc->format))
    {
        if (pAttachment->pDesc->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            clearAspect |= VK_IMAGE_ASPECT_COLOR_BIT;
        }
        else if ((m_subpassCount >= settings.minSubpassesForOptionalClears) &&
                 (pSubpass->bindTargets.colorTargetCount >= settings.minColorAttachmentsForOptionalClears) &&
                 (pAttachment->pDesc->loadOp == VK_ATTACHMENT_LOAD_OP_DONT_CARE))
        {
            // LOAD_OP_DONT_CARE color attachment are safe to clear, but make sure to use the optional flag to not
            // issue a slow clear.
            clearAspect |= VK_IMAGE_ASPECT_COLOR_BIT;
            isOptional = true;
        }
    }
    else
    {
        if (Formats::HasDepth(pAttachment->pDesc->format) &&
            pAttachment->pDesc->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            clearAspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        if (Formats::HasStencil(pAttachment->pDesc->format) &&
            pAttachment->pDesc->stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            clearAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }

    // Get how this attachment is referenced by its first use subpass
    const uint32_t refMask = GetSubpassReferenceMask(subpass, attachment);

    VK_ASSERT(refMask != 0);

    if (clearAspect != 0)
    {
        RPLoadOpClearInfo clearInfo = {};

        clearInfo.attachment = attachment;
        clearInfo.aspect     = clearAspect;
        clearInfo.isOptional = isOptional;

        // Load-op clear only if requested and the first reference isn't a resolve attachment (which will overwrite
        // the results of the clear and make it redundant).
        if (refMask != AttachRefResolveDst)
        {
            if (Formats::IsColorFormat(pAttachment->pDesc->format))
            {
                result = pSubpass->colorClears.PushBack(clearInfo);
            }
            else
            {
                result = pSubpass->dsClears.PushBack(clearInfo);
            }
        }
        else
        {
            // depth stencil resovle attachment will be cleared if depth/stencil resolve mode is none.
            if (((clearAspect & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                 (pSubpass->pDesc->depthResolveMode == VK_RESOLVE_MODE_NONE_KHR)) ||
                ((clearAspect & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                 (pSubpass->pDesc->stencilResolveMode == VK_RESOLVE_MODE_NONE_KHR)))
            {
                result = pSubpass->dsClears.PushBack(clearInfo);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function handles color attachment references within a subpass.  Called from BuildSubpass().
Pal::Result RenderPassBuilder::BuildColorAttachmentReferences(
    uint32_t                    subpass,
    const SubpassDescription&   desc)
{
    Pal::Result result     = Pal::Result::Success;
    SubpassState* pSubpass = &m_pSubpasses[subpass];

    pSubpass->bindTargets.colorTargetCount = 0;

    for (auto& target : pSubpass->bindTargets.colorTargets)
    {
        target.attachment        = VK_ATTACHMENT_UNUSED;
        target.layout.layout     = VK_IMAGE_LAYOUT_UNDEFINED;
        target.layout.extraUsage = 0;
    }

    if (desc.pColorAttachments != nullptr)
    {
        pSubpass->bindTargets.colorTargetCount = desc.colorAttachmentCount;

        for (uint32_t target = 0;
            (target < desc.colorAttachmentCount) && (result == Pal::Result::Success);
            ++target)
        {
            const AttachmentReference& reference = desc.pColorAttachments[target];

            RPImageLayout layout = { reference.layout, 0 };

            if (target < VK_ARRAY_SIZE(pSubpass->bindTargets.colorTargets))
            {
                pSubpass->bindTargets.colorTargets[target].attachment = reference.attachment;
                pSubpass->bindTargets.colorTargets[target].layout     = layout;
            }
            else
            {
                VK_NEVER_CALLED();
            }

            if ((result == Pal::Result::Success) && (reference.attachment != VK_ATTACHMENT_UNUSED))
            {
                result = TrackAttachmentUsage(
                    subpass,
                    AttachRefColor,
                    reference.attachment,
                    layout,
                    nullptr,
                    &pSubpass->syncTop);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function handles Variable Rate Shading references.  Called from BuildSubpass().
Pal::Result RenderPassBuilder::BuildFragmentShadingRateAttachmentReferences(
    uint32_t subpass)
{
    Pal::Result result = Pal::Result::Success;
    SubpassState* pSubpass = &m_pSubpasses[subpass];

    pSubpass->bindTargets.fragmentShadingRateTarget.attachment        = VK_ATTACHMENT_UNUSED;
    pSubpass->bindTargets.fragmentShadingRateTarget.layout.layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    pSubpass->bindTargets.fragmentShadingRateTarget.layout.extraUsage = 0;

    const AttachmentReference& reference = m_pInfo->pSubpasses[subpass].fragmentShadingRateAttachment;

    RPImageLayout layout = { reference.layout, 0 };

    if ((result == Pal::Result::Success) && (reference.attachment != VK_ATTACHMENT_UNUSED))
    {
        result = TrackAttachmentUsage(
            subpass,
            AttachRefFragShading,
            reference.attachment,
            layout,
            nullptr,
            &pSubpass->syncTop);

        pSubpass->bindTargets.fragmentShadingRateTarget.attachment = reference.attachment;
        pSubpass->bindTargets.fragmentShadingRateTarget.layout     = layout;
    }

    return result;
}

// =====================================================================================================================
// This function handles depth-stencil attachment references within a subpass.  Called from BuildSubpass().
Pal::Result RenderPassBuilder::BuildDepthStencilAttachmentReferences(
    uint32_t                    subpass,
    const SubpassDescription&   desc)
{
    Pal::Result result     = Pal::Result::Success;
    SubpassState* pSubpass = &m_pSubpasses[subpass];

    pSubpass->bindTargets.depthStencil.attachment               = VK_ATTACHMENT_UNUSED;
    pSubpass->bindTargets.depthStencil.layout.layout            = VK_IMAGE_LAYOUT_UNDEFINED;
    pSubpass->bindTargets.depthStencil.layout.extraUsage        = 0;
    pSubpass->bindTargets.depthStencil.stencilLayout.layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    pSubpass->bindTargets.depthStencil.stencilLayout.extraUsage = 0;

    if (desc.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED)
    {
        const AttachmentReference& reference = desc.depthStencilAttachment;

        if (reference.attachment != VK_ATTACHMENT_UNUSED)
        {
            RPImageLayout layout        = { reference.layout, 0 };
            RPImageLayout stencilLayout = { reference.stencilLayout, 0 };

            result = TrackAttachmentUsage(
                subpass,
                AttachRefDepthStencil,
                reference.attachment,
                layout,
                &stencilLayout,
                &pSubpass->syncTop);

            pSubpass->bindTargets.depthStencil.attachment    = reference.attachment;
            pSubpass->bindTargets.depthStencil.layout        = layout;
            pSubpass->bindTargets.depthStencil.stencilLayout = stencilLayout;
        }
    }

    return result;
}

// =====================================================================================================================
// This function handles input attachment references within a subpass.  Called from BuildSubpass().
Pal::Result RenderPassBuilder::BuildInputAttachmentReferences(
    uint32_t                    subpass,
    const SubpassDescription&   desc)
{
    Pal::Result result = Pal::Result::Success;
    SubpassState* pSubpass = &m_pSubpasses[subpass];

    // We only care about input attachments within a renderpass enough to make sure their layouts are transitioned
    // correctly; there's no actual "input attachment state" that needs to be programmed by a render pass instance
    // for our HW.
    if (desc.pInputAttachments != nullptr)
    {
        for (uint32_t target = 0;
            (target < desc.inputAttachmentCount) && (result == Pal::Result::Success);
            ++target)
        {
            const AttachmentReference& reference = desc.pInputAttachments[target];

            if (reference.attachment != VK_ATTACHMENT_UNUSED)
            {
                RPImageLayout layout        = { reference.layout, 0 };
                RPImageLayout stencilLayout = { reference.stencilLayout, 0 };

                result = TrackAttachmentUsage(
                    subpass,
                    AttachRefInput,
                    reference.attachment,
                    layout,
                    &stencilLayout,
                    &pSubpass->syncTop);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Handle resolve attachment references.  Called from BuildSubpass().
Pal::Result RenderPassBuilder::BuildResolveAttachmentReferences(
    uint32_t subpass)
{
    Pal::Result result = Pal::Result::Success;

    SubpassState* pSubpass                  = &m_pSubpasses[subpass];
    const SubpassDescription& subpassDesc   = *pSubpass->pDesc;

    if (subpassDesc.pResolveAttachments != nullptr)
    {
        for (uint32_t target = 0;
             (target < subpassDesc.colorAttachmentCount) && (result == Pal::Result::Success);
             ++target)
        {
            const AttachmentReference& src = subpassDesc.pColorAttachments[target];
            const AttachmentReference& dst = subpassDesc.pResolveAttachments[target];

            const RPImageLayout srcLayout = { src.layout, Pal::LayoutResolveSrc };
            const RPImageLayout dstLayout = { dst.layout, Pal::LayoutResolveDst };

            if ((dst.attachment != VK_ATTACHMENT_UNUSED) && (src.attachment != VK_ATTACHMENT_UNUSED))
            {
                result = TrackAttachmentUsage(
                    subpass,
                    AttachRefResolveSrc,
                    src.attachment,
                    srcLayout,
                    nullptr,
                    &pSubpass->syncPreResolve);

                if (result == Pal::Result::Success)
                {
                    result = TrackAttachmentUsage(
                        subpass,
                        AttachRefResolveDst,
                        dst.attachment,
                        dstLayout,
                        nullptr,
                        &pSubpass->syncPreResolve);
                }

                if (result == Pal::Result::Success)
                {
                    RPResolveInfo resolve = {};

                    resolve.src.attachment = src.attachment;
                    resolve.src.layout     = m_pAttachments[src.attachment].prevReferenceLayout;

                    resolve.dst.attachment = dst.attachment;
                    resolve.dst.layout     = m_pAttachments[dst.attachment].prevReferenceLayout;

                    result = pSubpass->resolves.PushBack(resolve);

                    VK_ASSERT(Formats::IsColorFormat(m_pAttachments[resolve.src.attachment].pDesc->format));
                    pSubpass->syncPreResolve.barrier.flags.preColorResolveSync = 1;

                    m_pAttachments[resolve.src.attachment].resolvesInFlight = true;
                    m_pAttachments[resolve.dst.attachment].resolvesInFlight = true;
                }
            }
        }
    }

    if ((subpassDesc.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) &&
        (subpassDesc.depthStencilResolveAttachment.attachment != VK_ATTACHMENT_UNUSED))
    {
        const AttachmentReference& src = subpassDesc.depthStencilAttachment;
        const AttachmentReference& dst = subpassDesc.depthStencilResolveAttachment;

        const RPImageLayout srcLayout        = { src.layout, Pal::LayoutResolveSrc };
        const RPImageLayout dstLayout        = { dst.layout, Pal::LayoutResolveDst };
        const RPImageLayout srcStencilLayout = { src.stencilLayout, Pal::LayoutResolveSrc };
        const RPImageLayout dstStencilLayout = { dst.stencilLayout, Pal::LayoutResolveDst };

            result = TrackAttachmentUsage(
                subpass,
                AttachRefResolveSrc,
                src.attachment,
                srcLayout,
                &srcStencilLayout,
                &pSubpass->syncPreResolve);

            if (result == Pal::Result::Success)
            {
                // Depth stencil resovle attachment will be cleared if depth/stencil resolve mode is none.
                const bool hasDepthClearOp   = (m_pAttachments[dst.attachment].pDesc->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
                                               (m_pSubpasses[subpass].pDesc->depthResolveMode == VK_RESOLVE_MODE_NONE_KHR);
                const bool hasStencilClearOp = (m_pAttachments[dst.attachment].pDesc->stencilLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) &&
                                               (m_pSubpasses[subpass].pDesc->stencilResolveMode == VK_RESOLVE_MODE_NONE_KHR);
                const bool hasClearOp        = (m_pAttachments[dst.attachment].loaded == false) && (hasDepthClearOp || hasStencilClearOp);

                // If depth stencil resovle attachment will be cleared, using top sync point to guarantee metadata init before clear.
                SyncPointState* pSync = hasClearOp ? &pSubpass->syncTop : &pSubpass->syncPreResolve;
                result = TrackAttachmentUsage(
                    subpass,
                    AttachRefResolveDst,
                    dst.attachment,
                    dstLayout,
                    &dstStencilLayout,
                    pSync);
            }

            if (result == Pal::Result::Success)
            {
                RPResolveInfo resolve = {};

                resolve.src.attachment    = src.attachment;
                resolve.src.layout        = m_pAttachments[src.attachment].prevReferenceLayout;
                resolve.src.stencilLayout = m_pAttachments[src.attachment].prevReferenceStencilLayout;

                resolve.dst.attachment    = dst.attachment;
                resolve.dst.layout        = m_pAttachments[dst.attachment].prevReferenceLayout;
                resolve.dst.stencilLayout = m_pAttachments[dst.attachment].prevReferenceStencilLayout;

                result = pSubpass->resolves.PushBack(resolve);

                VK_ASSERT(Formats::IsDepthStencilFormat(m_pAttachments[resolve.src.attachment].pDesc->format));
                pSubpass->syncPreResolve.barrier.flags.preDsResolveSync = 1;

                m_pAttachments[resolve.src.attachment].resolvesInFlight = true;
                m_pAttachments[resolve.dst.attachment].resolvesInFlight = true;
            }
        }

    return result;
}

// =====================================================================================================================
// This function builds the end-instance state of a render pass's execution state.
Pal::Result RenderPassBuilder::BuildEndState()
{
    // Build sync information based on external dependency leading out of the instance
    Pal::Result result = BuildSubpassDependencies(VK_SUBPASS_EXTERNAL, &m_endState.syncEnd);

    if (result == Pal::Result::Success)
    {
        result = BuildImplicitDependencies(VK_SUBPASS_EXTERNAL, &m_endState.syncEnd);
    }

    // Ensure that any pending resolves are done by the end of the render pass instance as a matter
    // of courtesy in case the app failed to add an external dependency.
    WaitForResolves(&m_endState.syncEnd);

    // Execute final layout changes.
    for (uint32_t a = 0; (a < m_attachmentCount) && (result == Pal::Result::Success); ++a)
    {
        const RPImageLayout finalLayout        = { m_pAttachments[a].pDesc->finalLayout, 0 };
        const RPImageLayout stencilFinalLayout = { m_pAttachments[a].pDesc->stencilFinalLayout, 0 };

        result = TrackAttachmentUsage(
            VK_SUBPASS_EXTERNAL,
            AttachRefExternalPostInstance,
            a,
            finalLayout,
            &stencilFinalLayout,
            &m_endState.syncEnd);
    }

    PostProcessSyncPoint(&m_endState.syncEnd);

    return result;
}

// =====================================================================================================================
static void IncludePipePoint(
    RPBarrierInfo*   pBarrier,
    Pal::HwPipePoint point)
{
    if (point == Pal::HwPipeBottom)
    {
        pBarrier->pipePointCount = 1;
        memset(pBarrier->pipePoints, 0, sizeof(pBarrier->pipePoints));
        pBarrier->pipePoints[0]  = Pal::HwPipeBottom;
    }
    else if ((pBarrier->pipePointCount == 0) ||
             (pBarrier->pipePoints[0] != Pal::HwPipeBottom))
    {
        bool seen = false;

        for (uint32_t i = 0; (i < pBarrier->pipePointCount) && (seen == false); ++i)
        {
            if (pBarrier->pipePoints[i] == point)
            {
                seen = true;
            }
        }

        if (seen == false)
        {
            VK_ASSERT(pBarrier->pipePointCount < VK_ARRAY_SIZE(pBarrier->pipePoints));

            pBarrier->pipePoints[pBarrier->pipePointCount++] = point;
        }
    }
}

// =====================================================================================================================
static void IncludeWaitPoint(
    RPBarrierInfo*   pBarrier,
    Pal::HwPipePoint point)
{
    if (point < pBarrier->waitPoint)
    {
        pBarrier->waitPoint = point;
    }
}

// =====================================================================================================================
static void ConvertImplicitSyncs(RPBarrierInfo* pBarrier)
{
    pBarrier->implicitSrcCacheMask = 0;
    pBarrier->implicitDstCacheMask = 0;

    // Similarly augment the waiting if we need to wait for prior color rendering to finish
    if (pBarrier->flags.preColorResolveSync ||
        pBarrier->flags.preDsResolveSync)
    {
        // If we're waiting prior a resolve, make sure the wait point waits early enough.
        IncludePipePoint(pBarrier, Pal::HwPipeBottom);
        IncludeWaitPoint(pBarrier, Pal::HwPipePreBlt);

        pBarrier->srcStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
        pBarrier->dstStageMask |= VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR;

        pBarrier->implicitSrcCacheMask |= pBarrier->flags.preColorResolveSync ?
                                          Pal::CoherColorTarget :
                                          Pal::CoherDepthStencilTarget;
        pBarrier->implicitDstCacheMask |= Pal::CoherResolveDst;
    }

    // Wait for (non-auto-synced) pre-clear if necessary.  No need to augment the pipe point because the prior work falls
    // under subpass dependency, but we may need to move the wait point forward to cover blts.
    if (pBarrier->flags.preColorClearSync ||
        pBarrier->flags.preDsClearSync)
    {
        IncludeWaitPoint(pBarrier, Pal::HwPipePreBlt);

        pBarrier->dstStageMask |= VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR;

        pBarrier->implicitDstCacheMask |= Pal::CoherClear;
    }

    // Augment the active source pipeline stages for resolves if we need to wait for prior resolves to complete
    if (pBarrier->flags.postResolveSync)
    {
        IncludePipePoint(pBarrier, Pal::HwPipePostBlt);
        IncludeWaitPoint(pBarrier, Pal::HwPipeTop);

        // Just going by the above wait point, the dstStageMask would be converted to TopOfPipe, but it is not optimal.
        // TopOfPipe causes a stall at PFP which is not really needed for images. As an optimization for Acq-Rel
        // barriers we instead set dstStage to Blt here.
        pBarrier->srcStageMask |= VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR;
        pBarrier->dstStageMask |= VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;

        pBarrier->implicitSrcCacheMask |= Pal::CoherResolveSrc;
    }
}

// =====================================================================================================================
// This function decides whether a sync point needs to actually execute any commands or if it's an empty sync point
// that can be skipped.
void RenderPassBuilder::PostProcessSyncPoint(
    SyncPointState* pSyncPoint)
{
    // Convert subpass dependency execution scope to PAL pipe/wait point
    pSyncPoint->barrier.waitPoint = VkToPalWaitPipePoint(pSyncPoint->barrier.dstStageMask);

    pSyncPoint->barrier.pipePointCount = VkToPalSrcPipePoints(pSyncPoint->barrier.srcStageMask,
                                                              pSyncPoint->barrier.pipePoints);

    // Include implicit waiting and cache access
    ConvertImplicitSyncs(&pSyncPoint->barrier);

    if (pSyncPoint->barrier.flags.implicitExternalOutgoing           &&
        (pSyncPoint->barrier.pipePointCount < (MaxHwPipePoints - 1)) &&
        m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetRuntimeSettings().implicitExternalSynchronization)
    {
        // Since there is no handling of implicitExternalIncoming today, make this visible immediately.
        IncludeWaitPoint(&pSyncPoint->barrier, Pal::HwPipeTop);

        pSyncPoint->barrier.pipePoints[pSyncPoint->barrier.pipePointCount] = Pal::HwPipeBottom;
        pSyncPoint->barrier.pipePointCount++;

        pSyncPoint->barrier.srcStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
        pSyncPoint->barrier.dstStageMask |= VK_PIPELINE_STAGE_2_BLIT_BIT_KHR;

        pSyncPoint->barrier.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    if (m_pDevice->GetPalProperties().gfxipProperties.flags.supportReleaseAcquireInterface &&
        m_pDevice->GetRuntimeSettings().useAcquireReleaseInterface)
    {
        // Need a global cache transition if any of the sync flags are set or if there's an app
        // subpass dependency that requires cache synchronization.
        if (((pSyncPoint->barrier.srcAccessMask != 0)         ||
             (pSyncPoint->barrier.dstAccessMask != 0)         ||
             (pSyncPoint->barrier.implicitSrcCacheMask != 0)  ||
             (pSyncPoint->barrier.implicitDstCacheMask != 0)) &&
            (pSyncPoint->transitions.NumElements() == 0))
        {
            // Need a global cache transition only if there are no image transitions.
            pSyncPoint->barrier.flags.needsGlobalTransition = 1;
        }

        // The barrier is active if it does any waiting or global cache synchronization or attachment transitions
        if ((pSyncPoint->barrier.pipePointCount > 0)          ||
            (pSyncPoint->barrier.flags.needsGlobalTransition) ||
            (pSyncPoint->transitions.NumElements() > 0))
        {
            pSyncPoint->flags.active = 1;

            if (pSyncPoint->barrier.dstStageMask == 0)
            {
                if (pSyncPoint->flags.top && (pSyncPoint->transitions.NumElements() > 0))
                {
                    // If a transition occurs when entering a subpass (top == 1), it must be synced before the
                    // attachment is accessed. If we're leaving the subpass, chances are there's another barrier down
                    // the line that will sync the image correctly.
                    pSyncPoint->barrier.dstStageMask = AllShaderStages;
                }
                else
                {
                    // BOTTOM_OF_PIPE in dst mask is effectively NONE.
                    pSyncPoint->barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR;
                }
            }

            // If srcSubpass for this barrier is VK_SUBPASS_EXTERNAL, srcStageMask is TOP_OF_PIPE and srcAccessMask is
            // 0 then this syncTop barrier might be doing a metadata Init with a layout transition out of undefined
            // layout. Set a flag here that can be tested later to set the srcStageMask correctly.
            const bool needsFixForMetaDataInit =
                ((pSyncPoint->flags.top)                                                       &&
                 (pSyncPoint->barrier.flags.explicitExternalIncoming)                          &&
                 (pSyncPoint->barrier.srcStageMask == VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR) &&
                 (pSyncPoint->barrier.srcAccessMask == 0));

            if ((pSyncPoint->barrier.srcStageMask == 0) || needsFixForMetaDataInit)
            {
                if (pSyncPoint->transitions.NumElements() > 0)
                {
                    // RPBarrierInfo consists of one set of src/dst stage masks which currently applies to each
                    // transition in RPSyncPoint(). PAL now supports specifying src/dst stage masks for each individual
                    // image transition. Since with this change we will loop over each transition to check for
                    // undefined 'prev' layout, there might be some cases where we add unnecessary stalls for at least
                    // some transitions.
                    for (auto it = pSyncPoint->transitions.Begin(); it.Get() != nullptr; it.Next())
                    {
                        RPTransitionInfo* info = it.Get();

                        if (info->prevLayout.layout == VK_IMAGE_LAYOUT_UNDEFINED)
                        {
                            pSyncPoint->barrier.srcStageMask |= pSyncPoint->barrier.dstStageMask;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Need a global cache transition if any of the sync flags are set or if there's an app
        // subpass dependency that requires cache synchronization.
        if ((pSyncPoint->barrier.srcAccessMask != 0)        ||
            (pSyncPoint->barrier.dstAccessMask != 0)        ||
            (pSyncPoint->barrier.implicitSrcCacheMask != 0) ||
            (pSyncPoint->barrier.implicitDstCacheMask != 0))
        {
            pSyncPoint->barrier.flags.needsGlobalTransition = 1;
        }

        // The barrier is active if it does any waiting or global cache synchronization or attachment transitions
        if ((pSyncPoint->barrier.pipePointCount > 0)          ||
            (pSyncPoint->barrier.flags.needsGlobalTransition) ||
            (pSyncPoint->transitions.NumElements() > 0))
        {
            pSyncPoint->flags.active = 1;
        }
    }
}

// =====================================================================================================================
// This function handles any implicit driver-required dependencies thay may be required prior to a particular subpass.
Pal::Result RenderPassBuilder::BuildImplicitDependencies(
    uint32_t        dstSubpass,
    SyncPointState* pSync)
{
    Pal::Result result = Pal::Result::Success;

    // We don't actually have any real implicit dependencies at the moment, and don't do much in this function.

    if (dstSubpass != VK_SUBPASS_EXTERNAL)
    {
        // Set the flag that this syncpoint needs to handle an implicit external incoming dependency as per spec.
        // Because of how we handle our memory dependency visibility, this flag doesn't actually need to do anything
        // at this time, but it's added in case we need it in the future.
        if (m_pSubpasses[dstSubpass].flags.hasExternalIncoming == false)
        {
            if (m_pSubpasses[dstSubpass].flags.hasFirstUseAttachments)
            {
                pSync->barrier.flags.implicitExternalIncoming = 1;
            }
        }
        else
        {
            pSync->barrier.flags.explicitExternalIncoming = 1;
        }
    }
    else
    {
        // Similarly, set the flag for requiring an external outgoing dependency.
        for (uint32_t srcSubpass = 0; (srcSubpass < m_subpassCount); ++srcSubpass)
        {
            if ((m_pSubpasses[srcSubpass].flags.hasExternalOutgoing == false) &&
                (m_pSubpasses[srcSubpass].flags.hasFinalUseAttachments))
            {
                pSync->barrier.flags.implicitExternalOutgoing = 1;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This function handles any synchronization from VkSubpassDependency.  Note that this includes
// subpass == VK_SUBPASS_EXTERNAL to handle the external-outgoing dependency.
Pal::Result RenderPassBuilder::BuildSubpassDependencies(
    uint32_t        subpass,
    SyncPointState* pSync)
{
    Pal::Result result = Pal::Result::Success;

    for (uint32_t d = 0; (d < m_pInfo->dependencyCount) && (result == Pal::Result::Success); ++d)
    {
        const SubpassDependency& dep = m_pInfo->pDependencies[d];

#if PAL_ENABLE_PRINTS_ASSERTS
        // Invalid dependency index
        if ((dep.srcSubpass != VK_SUBPASS_EXTERNAL) && (dep.srcSubpass >= m_subpassCount))
        {
            VK_NEVER_CALLED();
        }

        // Invalid dependency index
        if ((dep.dstSubpass != VK_SUBPASS_EXTERNAL) && (dep.dstSubpass >= m_subpassCount))
        {
            VK_NEVER_CALLED();
        }
#endif

        // If srcSubpass == dstSubpass, Vulkan spec calls this the subpass self-dependency and it has a special meaning.
        // It means that the app may call vkCmdPipelineBarriers inside the render pass (but they don't have to).
        // The driver should only do the barrier when vkCmdPipelineBarriers is called, not when starting the render pass.
        if (dep.srcSubpass == dep.dstSubpass)
        {
            continue;
        }

        // Does this dependency terminate at the current subpass?  If so, we need to handle it
        if (dep.dstSubpass == subpass)
        {
            pSync->barrier.srcStageMask |= dep.srcStageMask;
            pSync->barrier.dstStageMask |= dep.dstStageMask;
            pSync->barrier.srcAccessMask |= dep.srcAccessMask;
            pSync->barrier.dstAccessMask |= dep.dstAccessMask;

            // If there are currently resolve blts in flight, synchronize that they complete according to this
            // dependency.
            if (dep.srcSubpass != VK_SUBPASS_EXTERNAL)
            {
                WaitForResolvesFromSubpass(dep.srcSubpass, pSync);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// If the given subpass has resolves in flight for any attachment, this function will insert a barrier to wait for
// resolves to complete in the given sync point.
void RenderPassBuilder::WaitForResolvesFromSubpass(
    uint32_t        subpass,
    SyncPointState* pSync)
{
    for (uint32_t attachment = 0; attachment < m_attachmentCount; ++attachment)
    {
        if (m_pAttachments[attachment].resolvesInFlight &&
            m_pAttachments[attachment].prevReferenceSubpass == subpass)
        {
            // This waits for all resolves to complete via barrier.  We don't currently have split barrier support
            // for asynchronously waiting on resolves.
            WaitForResolves(pSync);

            break;
        }
    }
}

// =====================================================================================================================
// Returns true if any enabled bits in the ref mask are considered references that read from the attachment.
bool RenderPassBuilder::ReadsFromAttachment(
    uint32_t refMask)
{
    return ((refMask & (AttachRefInput | AttachRefResolveSrc)) != 0);
}

// =====================================================================================================================
// Returns true if any enabled bits in the ref mask are considered references that write to the attachment.
bool RenderPassBuilder::WritesToAttachment(
    uint32_t refMask)
{
    return ((refMask & (AttachRefColor | AttachRefDepthStencil | AttachRefResolveDst)) != 0);
}

// =====================================================================================================================
// This is a general function to track render pass usage of a particular attachment between subpasses.  It triggers
// automatic layout transitions as well as load-ops when that attachment is first used.
Pal::Result RenderPassBuilder::TrackAttachmentUsage(
    uint32_t             subpass,
    AttachRefType        refType,
    uint32_t             attachment,
    RPImageLayout        layout,
    const RPImageLayout* pStencilLayout,
    SyncPointState*      pSync)
{
    Pal::Result result = Pal::Result::Success;

    AttachmentState* pAttachment = &m_pAttachments[attachment];

    // This is a courtesy check, in case an application misses a dependency, to make sure that
    // an active resolve to this attachment is finished before attempting to use this attachment for anything
    // else
    if ((pAttachment->resolvesInFlight) && (subpass != pAttachment->prevReferenceSubpass))
    {
        VK_NEVER_CALLED();

        WaitForResolves(pSync);
    }

    // Detect if an automatic layout transition is needed and insert one to the given sync point if so.  Note that
    // these happen before load ops are triggered (below).
    if ((pAttachment->prevReferenceLayout != layout) ||
        ((pStencilLayout != nullptr) && (pAttachment->prevReferenceStencilLayout != *pStencilLayout)))
    {
        RPTransitionInfo transition = {};

        transition.attachment = attachment;
        transition.prevLayout = pAttachment->prevReferenceLayout;
        transition.nextLayout = layout;

        if (pStencilLayout != nullptr)
        {
            transition.prevStencilLayout = pAttachment->prevReferenceStencilLayout;
            transition.nextStencilLayout = *pStencilLayout;
        }

        if ((subpass != VK_SUBPASS_EXTERNAL) && (pAttachment->firstUseSubpass == subpass))
        {
            transition.flags.isInitialLayoutTransition = true;
        }

        // Add the transition
        result = pSync->transitions.PushBack(transition);

        // Track the current layout of this attachment
        pAttachment->prevReferenceLayout = layout;

        if (pStencilLayout != nullptr)
        {
            pAttachment->prevReferenceStencilLayout = *pStencilLayout;
        }
    }

    // Track how this attachment was last used
    pAttachment->prevReferenceSubpass  = subpass;
    pAttachment->accumulatedRefMask   |= refType;

    // Handle load ops for this attachment if this is the first time it is being used and it has not already
    // been loaded.
    if ((subpass != VK_SUBPASS_EXTERNAL) &&
        (pAttachment->firstUseSubpass == subpass) &&
        (pAttachment->loaded == false))
    {
        if (result == Pal::Result::Success)
        {
            result = BuildLoadOps(subpass, attachment);
        }
    }

    return result;
}

// =====================================================================================================================
// Waits for all resolves from any subpass that are still in flight to complete.  The wait happens in the given
// sync point.
void RenderPassBuilder::WaitForResolves(SyncPointState* pSync)
{
    for (uint32_t attachment = 0; attachment < m_attachmentCount; ++attachment)
    {
        if (m_pAttachments[attachment].resolvesInFlight)
        {
            pSync->barrier.flags.postResolveSync = 1;

            m_pAttachments[attachment].resolvesInFlight = false;
        }
    }
}

// =====================================================================================================================
// Finalizes the building of a render pass by compressing all of the temporary build-time memory into permanent
// structures that are retained by RenderPass objects.
Pal::Result RenderPassBuilder::Finalize(
    const VkAllocationCallbacks* pAllocator,
    RenderPassExecuteInfo**      ppResult
    ) const
{
    VK_ASSERT(pAllocator != nullptr);

    Pal::Result result = Pal::Result::Success;

    const size_t extraSize = GetTotalExtraSize();
    const size_t finalSize = sizeof(RenderPassExecuteInfo) + extraSize;

    void* pStorage      = nullptr;
    void* pStorageStart = nullptr;

    if (finalSize > 0)
    {
        pStorage = pAllocator->pfnAllocation(pAllocator->pUserData, finalSize,
                                             VK_DEFAULT_MEM_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        pStorageStart = pStorage;

        if (pStorage != nullptr)
        {
            memset(pStorage, 0, finalSize);
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    RenderPassExecuteInfo* pDst = nullptr;

    if (result == Pal::Result::Success)
    {
        pDst = reinterpret_cast<RenderPassExecuteInfo*>(pStorage);

        pStorage = Util::VoidPtrInc(pStorage, sizeof(RenderPassExecuteInfo));

        pDst->pSubpasses = static_cast<RPExecuteSubpassInfo*>(pStorage);

        pStorage = Util::VoidPtrInc(pStorage, m_subpassCount * sizeof(RPExecuteSubpassInfo));

        for (uint32_t s = 0; s < m_subpassCount; ++s)
        {
            pStorage = m_pSubpasses[s].Finalize(pStorage, &pDst->pSubpasses[s]);
        }

        pStorage = m_endState.Finalize(pStorage, &pDst->end);

        VK_ASSERT(Util::VoidPtrDiff(pStorage, pStorageStart) == finalSize);
    }

    if (result == Pal::Result::Success)
    {
        *ppResult = pDst;
    }
    else if (pStorageStart != nullptr)
    {
        pAllocator->pfnFree(pAllocator->pUserData, pStorageStart);
    }

    return result;
}

// =====================================================================================================================
size_t RenderPassBuilder::GetTotalExtraSize() const
{
    size_t finalSize = m_subpassCount * sizeof(RPExecuteSubpassInfo);

    for (uint32_t s = 0; s < m_subpassCount; ++s)
    {
        finalSize += m_pSubpasses[s].GetExtraSize();
    }

    finalSize += m_endState.GetExtraSize();

    return finalSize;
}

// =====================================================================================================================
RenderPassBuilder::SubpassState::SubpassState(
    const SubpassDescription*   pDesc,
    utils::TempMemArena*        pArena)
    :
    pDesc(pDesc),
    syncTop(pArena),
    colorClears(pArena),
    dsClears(pArena),
    syncPreResolve(pArena),
    resolves(pArena),
    syncBottom(pArena)
{
    flags.u32All = 0;

    memset(&bindTargets, 0, sizeof(bindTargets));

    syncTop.flags.top = 1;
}

// =====================================================================================================================
RenderPassBuilder::SubpassState::~SubpassState()
{
    while (colorClears.NumElements() > 0)
    {
        auto it = colorClears.Begin();

        colorClears.Erase(&it);
    }

    while (dsClears.NumElements() > 0)
    {
        auto it = dsClears.Begin();

        dsClears.Erase(&it);
    }

    while (resolves.NumElements() > 0)
    {
        auto it = resolves.Begin();

        resolves.Erase(&it);
    }
}

// =====================================================================================================================
size_t RenderPassBuilder::SubpassState::GetExtraSize() const
{
    size_t extraSize = 0;

    extraSize += syncTop.GetExtraSize();
    extraSize += colorClears.NumElements() * sizeof(RPLoadOpClearInfo);
    extraSize += dsClears.NumElements() * sizeof(RPLoadOpClearInfo);
    extraSize += syncPreResolve.GetExtraSize();
    extraSize += resolves.NumElements() * sizeof(RPResolveInfo);
    extraSize += syncBottom.GetExtraSize();

    return extraSize;
}

// =====================================================================================================================
template<typename T>
void* AssignArray(size_t n, void* pStorage, uint32_t* pArraySize, T** ppDest)
{
    *pArraySize = static_cast<uint32_t>(n);

    if (n > 0)
    {
        *ppDest = reinterpret_cast<T*>(pStorage);

        pStorage = Util::VoidPtrInc(pStorage, sizeof(T) * n);
    }
    else
    {
        *ppDest = nullptr;
    }

    return pStorage;
}

// =====================================================================================================================
void* RenderPassBuilder::SubpassState::Finalize(
    void*                 pStorage,
    RPExecuteSubpassInfo* pSubpass
    ) const
{
    memset(pSubpass, 0, sizeof(*pSubpass));

    auto* pBegin = &pSubpass->begin;

    pStorage = syncTop.Finalize(pStorage, &pBegin->syncTop);

    pStorage = AssignArray(colorClears.NumElements(), pStorage,
        &pBegin->loadOps.colorClearCount, &pBegin->loadOps.pColorClears);

    uint32_t colorIdx = 0;

    for (auto it = colorClears.Begin(); it.Get() != nullptr; it.Next())
    {
        pBegin->loadOps.pColorClears[colorIdx++] = *it.Get();
    }

    pStorage = AssignArray(dsClears.NumElements(), pStorage,
        &pBegin->loadOps.dsClearCount, &pBegin->loadOps.pDsClears);

    uint32_t dsIdx = 0;

    for (auto it = dsClears.Begin(); it.Get() != nullptr; it.Next())
    {
        pBegin->loadOps.pDsClears[dsIdx++] = *it.Get();
    }

    pBegin->bindTargets = bindTargets;

    auto* pEnd = &pSubpass->end;

    pStorage = syncPreResolve.Finalize(pStorage, &pEnd->syncPreResolve);

    pStorage = AssignArray(resolves.NumElements(), pStorage, &pEnd->resolveCount, &pEnd->pResolves);

    uint32_t resolveIdx = 0;

    for (auto it = resolves.Begin(); it.Get() != nullptr; it.Next())
    {
        pEnd->pResolves[resolveIdx++] = *it.Get();
    }

    pStorage = syncBottom.Finalize(pStorage, &pEnd->syncBottom);

    return pStorage;
}

// =====================================================================================================================
size_t RenderPassBuilder::SyncPointState::GetExtraSize() const
{
    size_t extraSize = 0;

    extraSize += transitions.NumElements() * sizeof(RPTransitionInfo);

    return extraSize;
}

// =====================================================================================================================
RenderPassBuilder::SyncPointState::SyncPointState(
    utils::TempMemArena* pArena)
    :
    transitions(pArena)
{
    flags.u32All = 0;

    memset(&barrier, 0, sizeof(barrier));
}

// =====================================================================================================================
RenderPassBuilder::SyncPointState::~SyncPointState()
{
    while (transitions.NumElements() > 0)
    {
        auto it = transitions.Begin();

        transitions.Erase(&it);
    }
}

// =====================================================================================================================
void* RenderPassBuilder::SyncPointState::Finalize(
    void*            pStorage,
    RPSyncPointInfo* pSyncPoint
    ) const
{
    pSyncPoint->flags   = flags;
    pSyncPoint->barrier = barrier;

    pStorage = AssignArray(transitions.NumElements(), pStorage, &pSyncPoint->transitionCount,
        &pSyncPoint->pTransitions);

    uint32_t tIdx = 0;

    for (auto it = transitions.Begin(); it.Get() != nullptr; it.Next())
    {
        pSyncPoint->pTransitions[tIdx++] = *it.Get();
    }

    return pStorage;
}

// =====================================================================================================================
RenderPassBuilder::EndState::EndState(
    utils::TempMemArena* pArena)
    :
    syncEnd(pArena)
{

}

// =====================================================================================================================
size_t RenderPassBuilder::EndState::GetExtraSize() const
{
    size_t extraSize = 0;

    extraSize += syncEnd.GetExtraSize();

    return extraSize;
}

// =====================================================================================================================
void* RenderPassBuilder::EndState::Finalize(
    void*                       pStorage,
    RPExecuteEndRenderPassInfo* pEndState
    ) const
{
    pStorage = syncEnd.Finalize(pStorage, &pEndState->syncEnd);

    return pStorage;
}

}; // namespace vk
