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

#ifndef __RENDERPASS_RENDERPASS_TYPES_H__
#define __RENDERPASS_RENDERPASS_TYPES_H__
#pragma once

#include <limits.h>

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_dispatch.h"

#include "palVector.h"

namespace vk
{

// Image layout structure to describe a render pass attachment's layout in a subpass.  It is basically the same as
// a VkImageLayout with some additional internal flags.
struct RPImageLayout
{
    VkImageLayout layout;       // Base Vulkan image layout
    uint32_t      extraUsage;   // Extra PAL layout usages (used to e.g. make attachments resolve-compatible)

    bool operator==(const RPImageLayout& rhs) const
        { return (layout == rhs.layout) && (extraUsage == rhs.extraUsage); }

    bool operator!=(const RPImageLayout& rhs) const
        { return (layout != rhs.layout) || (extraUsage != rhs.extraUsage); }
};

// Describes an {attachment, layout} pair.  Analogous to VkAttachmentReference.
struct RPAttachmentReference
{
    uint32_t           attachment;
    RPImageLayout      layout;
    RPImageLayout      stencilLayout;
};

// Describes information about an automatic layout transition happening inside a render pass instance.
struct RPTransitionInfo
{
    uint32_t      attachment;        // Attachment being transitioned
    RPImageLayout prevLayout;        // Previous layout
    RPImageLayout nextLayout;        // Next layout
    RPImageLayout prevStencilLayout; // Previous stencil layout
    RPImageLayout nextStencilLayout; // Next stencil layout

    union
    {
        struct
        {
            uint32_t isInitialLayoutTransition :  1;
            uint32_t reserved                  : 31;
        };
        uint32_t u32All;
    } flags;
};

// Information about a load-op clear to be done on a particular attachment (either color or depth/stencil)
struct RPLoadOpClearInfo
{
    uint32_t           attachment; // Attachment to be cleared
    VkImageAspectFlags aspect;     // Which image aspects are to be cleared
    bool               isOptional; // If possible, fast clear in case data is not well compressed
};

// Information about a resolve operation due to a resolve attachment
struct RPResolveInfo
{
    RPAttachmentReference src; // Attachment to resolve from
    RPAttachmentReference dst; // Attachment to resolve to
};

// Information about which color/depth-stencil targets are bound for a subpass's contents
struct RPBindTargetsInfo
{
    uint32_t              colorTargetCount;
    RPAttachmentReference colorTargets[Pal::MaxColorTargets];
    RPAttachmentReference depthStencil;
    RPAttachmentReference fragmentShadingRateTarget;
};

// Information about any necessary barrier operations done during an RPSyncPoint.  Includes composite
// VkSubpassDependency contributions, but also flags to do certain internal special synchronizations.
struct RPBarrierInfo
{
    // The following fields are a composite of all VkSubpassDependencies that affect this particular barrier:
    PipelineStageFlags   srcStageMask;
    PipelineStageFlags   dstStageMask;
    AccessFlags          srcAccessMask;
    AccessFlags          dstAccessMask;
    Pal::HwPipePoint     waitPoint;
    uint32_t             pipePointCount;
    Pal::HwPipePoint     pipePoints[MaxHwPipePoints];
    uint32_t             implicitSrcCacheMask;
    uint32_t             implicitDstCacheMask;

    union
    {
        struct
        {
            uint32_t needsGlobalTransition    : 1; // True if the barrier needs a global (non-attachment) barrier
                                                   // transition during execution.  This is a master bit that is
                                                   // calculated from the more precise sync needs of this barrier.
            uint32_t implicitExternalIncoming : 1; // Hint that this barrier includes an implicit incoming subpass
                                                   // dependency because no explicit external subpass dependency was
                                                   // provided, per spec-rules.
            uint32_t implicitExternalOutgoing : 1; // Hint that this barrier includes an implicit incoming subpass
                                                   // dependency because no explicit external subpass dependency was
                                                   // provided, per spec-rules.
            uint32_t preColorResolveSync      : 1; // Barrier needs to synchronize prior color writes against an
                                                   // impending color resolve.
            uint32_t preDsResolveSync         : 1; // Barrier needs to synchronize against prior depth-stencil writes
                                                   // against an impending depth-stencil resolve.
            uint32_t postResolveSync          : 1; // Barrier needs to synchronize against a prior resolve operation.
            uint32_t preColorClearSync        : 1; // Barrier needs to synchronize before an impending load-op color
                                                   // clear.
            uint32_t preDsClearSync           : 1; // Barrier needs to synchronize before an impending load-op
                                                   // depth/stencil clear.
            uint32_t explicitExternalIncoming : 1; // Hint that this barrier has an explicit incoming subpass
                                                   // dependency and we might need to update some barrier flags for
                                                   // meta data init correctness based on this one.
            uint32_t reserved                 : 23;
        };
        uint32_t u32All;
    } flags;
};

union RPSyncPointFlags
{
    struct
    {
        uint32_t active   : 1;  // True if this sync point needs to be handled
        uint32_t top      : 1;  // True if this is the top sync point

        uint32_t reserved : 30;
    };
    uint32_t u32All;
};

union SubpassStateFlags
{
    struct
    {
        uint32_t hasFirstUseAttachments  :  1; // True if this subpass has first-use references
        uint32_t hasFinalUseAttachments  :  1; // Same as above, but final-use.
        uint32_t hasExternalIncoming     :  1; // True if an explicit VkSubpassDependency exists with src =
                                               // VK_SUBPASS_EXTERNAL and dst = this.
        uint32_t hasExternalOutgoing     :  1; // Same as above, but src and dst reversed.
        uint32_t reserved1               :  2;
        uint32_t reserved                : 26;
    };
    uint32_t u32All;
};

// This is a render pass "synchronization point" that mainly translates to a barrier.  Any synchronization across
// subpasses, or between different parts of the same subpass (e.g. pre/post resolve) happens within a synchronization
// point.  Also any layout transitions are executed within a synchronization point.
struct RPSyncPointInfo
{
    RPBarrierInfo      barrier;
    RPSyncPointFlags   flags;
    uint32_t           transitionCount;
    RPTransitionInfo*  pTransitions;
};

// Describes steps that need to be done during the "beginning" of a subpass i.e. during RPBeginSubpass().
//
// The operations are executed more or less in the order they appear in this structure.
struct RPExecuteBeginSubpassInfo
{
    // Synchronization happening at the top of a subpass (before any clears)
    RPSyncPointInfo        syncTop;

    // Operations required by load ops happening at the top of this subpass
    struct
    {
        // Color clears happening at the top of a subpass
        uint32_t           colorClearCount;
        RPLoadOpClearInfo* pColorClears;

        // DS clears happening at the top of a subpass
        uint32_t           dsClearCount;
        RPLoadOpClearInfo* pDsClears;
    } loadOps;

    // Target bind information
    RPBindTargetsInfo      bindTargets;
};

// Describes steps that need to be done during the "end" of a subpass i.e. during RPBeginSubpass().
//
// The operations are executed more or less in the order they appear in this structure.
struct RPExecuteEndSubpassInfo
{
    // Synchronization happening after subpass rendering, but prior to any resolves
    RPSyncPointInfo    syncPreResolve;

    // Resolves happening at the bottom of a subpass
    uint32_t           resolveCount;
    RPResolveInfo*     pResolves;

    // Sync point at the bottom of the subpass
    RPSyncPointInfo    syncBottom;
};

// Describes information required to execute the internal operations to set-up a subpass.  These are split to the
// "beginning" of a subpass and the "end" of a subpass.  A subpass is ended during vkCmdNextSubpass/vkCmdEndRenderPass
// before the next subpass is "begun".
struct RPExecuteSubpassInfo
{
    RPExecuteBeginSubpassInfo begin;
    RPExecuteEndSubpassInfo   end;
};

// Describes information uniquely required to be done at the end of a render pass.
//
// Executed during vkCmdEndRenderPass().
struct RPExecuteEndRenderPassInfo
{
    RPSyncPointInfo      syncEnd; // Synchronization that needs to be done during the end of a render pass instance.
};

// The main structure that describes all information necessary to execute an instance of some render pass (except
// for the subpass contents).
struct RenderPassExecuteInfo
{
    RPExecuteSubpassInfo*      pSubpasses;
    RPExecuteEndRenderPassInfo end;
};

} // namespace vk

#endif /* __RENDERPASS_RENDERPASS_TYPES_H__ */
