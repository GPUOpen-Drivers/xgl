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

#ifndef __RENDERPASS_RENDERPASS_BUILDER_H__
#define __RENDERPASS_RENDERPASS_BUILDER_H__
#pragma once

#include <limits.h>

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_dispatch.h"
#include "include/vk_framebuffer.h"
#include "utils/temp_mem_arena.h"

#include "palList.h"
#include "palVector.h"

namespace vk
{

class CmdBuffer;
class Device;
class Framebuffer;
class ImageView;
class Instance;
class RenderPassCmdList;
struct RenderPassActiveState;
struct RuntimeSettings;
class RenderPass;
struct RenderPassCreateInfo;
struct AttachmentDescription;
struct SubpassDescription;

// =====================================================================================================================
// This class is a temporarily instantiated class that builds a RenderPassExecuteInfo during vkCreateRenderPass().
class RenderPassBuilder
{
public:
    // Flags of different types of attachment references.
    enum AttachRefType
    {
        AttachRefExternalPreInstance  = 0x00000001, // Dummy flag denoting pre-instance reference
        AttachRefColor                = 0x00000002, // Color attachment
        AttachRefInput                = 0x00000004, // Input attachment
        AttachRefDepthStencil         = 0x00000008, // Depth-stencil attachment
        AttachRefResolveSrc           = 0x00000010, // Color attachment used as a resolve source
        AttachRefResolveDst           = 0x00000020, // Resolve attachment
        AttachRefPreserve             = 0x00000040, // Preserve attachment (not really used)
        AttachRefExternalPostInstance = 0x00000080, // Dummy flag denoting post-instance reference
        AttachRefFragShading          = 0x00000100  // Fragment shading rate attachment
    };

    // State tracked per attachment during building
    struct AttachmentState
    {
        AttachmentState(const AttachmentDescription* pDesc);

        const AttachmentDescription*   pDesc;

        uint32_t                       firstUseSubpass;             // Subpass that first references this attachment
        uint32_t                       finalUseSubpass;             // Subpass that lasts references this attachment

        RPImageLayout                  prevReferenceLayout;         // Layout used by previous reference
        RPImageLayout                  prevReferenceStencilLayout;  // Stencil layout used by previous reference if any
        uint32_t                       prevReferenceSubpass;        // Previously-referencing subpass index
        uint32_t                       accumulatedRefMask;          // Accumulating mask of what kinds of AttachRef*
                                                                    // flags have so far referenced this attachment.
        bool                           loaded;                      // True if attachment has been loaded.
        bool                           resolvesInFlight;            // True if a resolve blt is in flight either
                                                                    // from or to this attachment.
    };

    // State tracked per subpass sync point (build-time version of RPSyncPoint).
    struct SyncPointState
    {
        SyncPointState(utils::TempMemArena* pArena);
        ~SyncPointState();

        size_t GetExtraSize() const;
        void* Finalize(void* pStorage, RPSyncPointInfo* pResult) const;

        RPSyncPointFlags                                  flags;
        RPBarrierInfo                                     barrier;
        Util::List<RPTransitionInfo, utils::TempMemArena> transitions;
    };

    // State tracked per subpass during building (build-time version of RPExecuteSubpassInfo)
    struct SubpassState
    {
        SubpassState(const SubpassDescription* pDesc, utils::TempMemArena* pArena);
        ~SubpassState();

        size_t GetExtraSize() const;
        void* Finalize(void* pStorage, RPExecuteSubpassInfo* pResult) const;

        const SubpassDescription*                          pDesc;

        // Build-time state for RPExecuteBeginSubpassInfo:
        SyncPointState                                     syncTop;
        Util::List<RPLoadOpClearInfo, utils::TempMemArena> colorClears;
        Util::List<RPLoadOpClearInfo, utils::TempMemArena> dsClears;
        RPBindTargetsInfo                                  bindTargets;
        SyncPointState                                     syncPreResolve;
        Util::List<RPResolveInfo, utils::TempMemArena>     resolves;

        // Build-time state for RPExecuteEndSubpassInfo:
        SyncPointState                                     syncBottom;

        SubpassStateFlags                                  flags;
    };

    // State tracked for the end-instance state during building (analogous to RPExecuteEndState).
    struct EndState
    {
        EndState(utils::TempMemArena* pArena);

        size_t GetExtraSize() const;
        void* Finalize(void* pStorage, RPExecuteEndRenderPassInfo* pResult) const;

        SyncPointState syncEnd;
    };

    RenderPassBuilder(Device* pDevice, utils::TempMemArena* pArena);
    ~RenderPassBuilder();

    VkResult Build(
        const RenderPassCreateInfo*     pRenderPassInfo,
        const VkAllocationCallbacks*    pAllocator,
        RenderPassExecuteInfo**         ppResult);

    const RenderPassCreateInfo* GetInfo() const
        { return m_pInfo; }

    const utils::TempMemArena* GetArena() const
        { return m_pArena; }

    const AttachmentState* GetAttachment(uint32_t a) const
        { return &m_pAttachments[a]; }

    const SubpassState* GetSubpass(uint32_t s) const
        { return &m_pSubpasses[s]; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(RenderPassBuilder);

    Pal::Result BuildInitialState();
    Pal::Result BuildSubpass(uint32_t subpass);
    Pal::Result BuildSubpassDependencies(uint32_t subpass, SyncPointState* pSync);
    Pal::Result BuildImplicitDependencies(uint32_t subpass, SyncPointState* pSync);
    Pal::Result BuildLoadOps(uint32_t subpass, uint32_t attachment);
    Pal::Result BuildColorAttachmentReferences(uint32_t subpass, const SubpassDescription& desc);
    Pal::Result BuildDepthStencilAttachmentReferences(uint32_t subpass, const SubpassDescription& desc);
    Pal::Result BuildInputAttachmentReferences(uint32_t subpass, const SubpassDescription& desc);
    Pal::Result BuildResolveAttachmentReferences(uint32_t subpass);
    Pal::Result BuildFragmentShadingRateAttachmentReferences(uint32_t subpass);
    Pal::Result BuildSamplePatternMemoryStore(uint32_t attachment);
    Pal::Result BuildEndState();
    Pal::Result TrackAttachmentUsage(
        uint32_t             subpass,
        AttachRefType        refType,
        uint32_t             attachment,
        RPImageLayout        layout,
        const RPImageLayout* pStencilLayout,
        SyncPointState*      pSync);
    void WaitForResolves(SyncPointState* pSync);
    void WaitForResolvesFromSubpass(uint32_t subpass, SyncPointState* pSync);

    Pal::Result Finalize(
        const VkAllocationCallbacks* pAllocator,
        RenderPassExecuteInfo**      ppResult) const;
    void Cleanup();
    uint32_t GetSubpassReferenceMask(uint32_t subpass, uint32_t attachment) const;
    static bool ReadsFromAttachment(uint32_t refMask);
    static bool WritesToAttachment(uint32_t refMask);
    void PostProcessSyncPoint(SyncPointState* pSyncPoint);
    size_t GetTotalExtraSize() const;

    const RenderPassCreateInfo*              m_pInfo;               // Internal create info
    Device* const                            m_pDevice;             // Device pointer
    utils::TempMemArena*                     m_pArena;              // Arena for allocating build-time scratch memory
    uint32_t                                 m_attachmentCount;     // Attachment count
    AttachmentState*                         m_pAttachments;        // Per-attachment build state
    uint32_t                                 m_subpassCount;        // Subpass count
    SubpassState*                            m_pSubpasses;          // Per-subpass build state
    EndState                                 m_endState;            // End-instance build state
};

} // namespace vk

#endif /* __RENDERPASS_RENDERPASS_BUILDER_H__ */
