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

#include <cmath>

#include "include/vk_conv.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_render_pass.h"
#include "include/vk_framebuffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_utils.h"

#include "palVectorImpl.h"
#include "palMetroHash.h"

namespace vk
{

// =====================================================================================================================
static void GenerateHashFromAttachmentDescription(
    Util::MetroHash64*              pHasher,
    const AttachmentDescription&    desc)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.format);
    pHasher->Update(desc.samples);
    pHasher->Update(desc.loadOp);
    pHasher->Update(desc.storeOp);
    pHasher->Update(desc.stencilLoadOp);
    pHasher->Update(desc.stencilStoreOp);
    pHasher->Update(desc.initialLayout);
    pHasher->Update(desc.finalLayout);
    pHasher->Update(desc.stencilInitialLayout);
    pHasher->Update(desc.stencilFinalLayout);
}

// =====================================================================================================================
static void GenerateHashFromAttachmentReference(
    Util::MetroHash64*              pHasher,
    const AttachmentReference&      desc)
{
    pHasher->Update(desc.attachment);
    pHasher->Update(desc.layout);
    pHasher->Update(desc.stencilLayout);
    pHasher->Update(desc.aspectMask);
}

// =====================================================================================================================
static void GenerateHashForSubpassAttachment(
    Util::MetroHash64*          pHasher,
    const RenderPassCreateInfo* pRenderPassInfo,
    const AttachmentReference&  desc)
{
    pHasher->Update(desc.aspectMask);
    if (desc.attachment != VK_ATTACHMENT_UNUSED)
    {
        auto pAttachment = &pRenderPassInfo->pAttachments[desc.attachment];
        pHasher->Update(pAttachment->format);
        pHasher->Update(pAttachment->samples);
    }
}

// =====================================================================================================================
static void GenerateHashFromSubpassDependency(
    Util::MetroHash64*              pHasher,
    const SubpassDependency&        desc)
{
    pHasher->Update(desc.srcSubpass);
    pHasher->Update(desc.dstSubpass);
    pHasher->Update(desc.srcStageMask);
    pHasher->Update(desc.dstStageMask);
    pHasher->Update(desc.srcAccessMask);
    pHasher->Update(desc.dstAccessMask);
    pHasher->Update(desc.dependencyFlags);
    pHasher->Update(desc.viewOffset);
}

// =====================================================================================================================
static void GenerateHashFromSubpassDescription(
    Util::MetroHash64*          pHasher,
    const SubpassDescription&   desc)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.pipelineBindPoint);
    pHasher->Update(desc.viewMask);
    pHasher->Update(desc.inputAttachmentCount);
    pHasher->Update(desc.colorAttachmentCount);
    pHasher->Update(desc.preserveAttachmentCount);
    GenerateHashFromAttachmentReference(pHasher, desc.depthStencilAttachment);
    GenerateHashFromAttachmentReference(pHasher, desc.depthStencilResolveAttachment);
    GenerateHashFromAttachmentReference(pHasher, desc.fragmentShadingRateAttachment);
    pHasher->Update(desc.subpassSampleCount);

    for (uint32_t i = 0; i < desc.inputAttachmentCount; ++i)
    {
        GenerateHashFromAttachmentReference(pHasher, desc.pInputAttachments[i]);
    }

    for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i)
    {
        GenerateHashFromAttachmentReference(pHasher, desc.pColorAttachments[i]);
    }

    if (desc.pResolveAttachments != nullptr)
    {
        for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i)
        {
            GenerateHashFromAttachmentReference(pHasher, desc.pResolveAttachments[i]);
        }
    }

    if (desc.preserveAttachmentCount > 0)
    {
        pHasher->Update(
            reinterpret_cast<const uint8_t*>(desc.pPreserveAttachments),
            static_cast<uint64_t>(desc.preserveAttachmentCount * sizeof(uint32_t)));
    }
}

// =====================================================================================================================
static uint64_t GenerateRenderPassHash(
    const RenderPassCreateInfo* pRenderPassInfo)
{
    Util::MetroHash64 hasher;

    hasher.Update(pRenderPassInfo->flags);
    hasher.Update(pRenderPassInfo->attachmentCount);
    hasher.Update(pRenderPassInfo->subpassCount);
    hasher.Update(pRenderPassInfo->dependencyCount);

    for (uint32_t i = 0; i < pRenderPassInfo->attachmentCount; ++i)
    {
        GenerateHashFromAttachmentDescription(&hasher, pRenderPassInfo->pAttachments[i]);
    }

    for (uint32_t i = 0; i < pRenderPassInfo->dependencyCount; ++i)
    {
        GenerateHashFromSubpassDependency(&hasher, pRenderPassInfo->pDependencies[i]);
    }

    for (uint32_t i = 0; i < pRenderPassInfo->subpassCount; ++i)
    {
        GenerateHashFromSubpassDescription(&hasher, pRenderPassInfo->pSubpasses[i]);
    }

    if (pRenderPassInfo->correlatedViewMaskCount > 0)
    {
        hasher.Update(
            reinterpret_cast<const uint8_t*>(pRenderPassInfo->pCorrelatedViewMasks),
            static_cast<uint64_t>(pRenderPassInfo->correlatedViewMaskCount * sizeof(uint32_t)));
    }

    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t*>(&hash));

    return hash;
}

// =====================================================================================================================
static uint64_t GenerateSubpassHash(
    const RenderPassCreateInfo* pRenderPassInfo,
    uint32_t                    subpass)
{
    Util::MetroHash64 hasher;
    const SubpassDescription& subpassDesc = pRenderPassInfo->pSubpasses[subpass];

    hasher.Update(subpassDesc.viewMask);
    hasher.Update(subpassDesc.inputAttachmentCount);
    hasher.Update(subpassDesc.colorAttachmentCount);
    hasher.Update(subpassDesc.subpassSampleCount);

    GenerateHashForSubpassAttachment(&hasher, pRenderPassInfo, subpassDesc.depthStencilAttachment);

    for (uint32_t i = 0; i < subpassDesc.inputAttachmentCount; ++i)
    {
        GenerateHashForSubpassAttachment(&hasher, pRenderPassInfo, subpassDesc.pInputAttachments[i]);
    }

    for (uint32_t i = 0; i < subpassDesc.colorAttachmentCount; ++i)
    {
        GenerateHashForSubpassAttachment(&hasher, pRenderPassInfo, subpassDesc.pColorAttachments[i]);
    }
    uint64_t hash;
    hasher.Finalize(reinterpret_cast<uint8_t*>(&hash));
    return hash;
}
// =====================================================================================================================
AttachmentReference::AttachmentReference()
    :
    attachment   (VK_ATTACHMENT_UNUSED),
    layout       (VK_IMAGE_LAYOUT_UNDEFINED),
    stencilLayout(VK_IMAGE_LAYOUT_UNDEFINED),
    aspectMask   (VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM)
{
}

// =====================================================================================================================
void AttachmentReference::Init(const VkAttachmentReference& attachRef)
{
    attachment    = attachRef.attachment;
    layout        = attachRef.layout;
    stencilLayout = attachRef.layout;
    aspectMask    = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
}

// =====================================================================================================================
void AttachmentReference::Init(const VkAttachmentReference2& attachRef)
{
    attachment    = attachRef.attachment;
    layout        = attachRef.layout;
    aspectMask    = attachRef.aspectMask;
    stencilLayout = attachRef.layout;

    const void* pNext = attachRef.pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT:
        {
            const auto* pExtInfo = static_cast<const VkAttachmentReferenceStencilLayout*>(pNext);
            stencilLayout = pExtInfo->stencilLayout;

            break;
        }
        default:
            // Skip any unknown extension structures.
            break;
        }

        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
AttachmentDescription::AttachmentDescription()
    :
    flags               (0),
    format              (VK_FORMAT_UNDEFINED),
    samples             (VK_SAMPLE_COUNT_1_BIT),
    loadOp              (VK_ATTACHMENT_LOAD_OP_DONT_CARE),
    storeOp             (VK_ATTACHMENT_STORE_OP_DONT_CARE),
    stencilLoadOp       (VK_ATTACHMENT_LOAD_OP_DONT_CARE),
    stencilStoreOp      (VK_ATTACHMENT_STORE_OP_DONT_CARE),
    initialLayout       (VK_IMAGE_LAYOUT_UNDEFINED),
    finalLayout         (VK_IMAGE_LAYOUT_UNDEFINED),
    stencilInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED),
    stencilFinalLayout  (VK_IMAGE_LAYOUT_UNDEFINED)
{
}

// =====================================================================================================================
void AttachmentDescription::Init(const VkAttachmentDescription& attachDesc)
{
    flags                = attachDesc.flags;
    format               = attachDesc.format;
    samples              = attachDesc.samples;
    loadOp               = attachDesc.loadOp;
    storeOp              = attachDesc.storeOp;
    stencilLoadOp        = attachDesc.stencilLoadOp;
    stencilStoreOp       = attachDesc.stencilStoreOp;
    initialLayout        = attachDesc.initialLayout;
    finalLayout          = attachDesc.finalLayout;
    stencilInitialLayout = attachDesc.initialLayout;
    stencilFinalLayout   = attachDesc.finalLayout;
}

// =====================================================================================================================
void AttachmentDescription::Init(const VkAttachmentDescription2& attachDesc)
{
    flags                = attachDesc.flags;
    format               = attachDesc.format;
    samples              = attachDesc.samples;
    loadOp               = attachDesc.loadOp;
    storeOp              = attachDesc.storeOp;
    stencilLoadOp        = attachDesc.stencilLoadOp;
    stencilStoreOp       = attachDesc.stencilStoreOp;
    initialLayout        = attachDesc.initialLayout;
    finalLayout          = attachDesc.finalLayout;
    stencilInitialLayout = attachDesc.initialLayout;
    stencilFinalLayout   = attachDesc.finalLayout;

    const void* pNext = attachDesc.pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT:
        {
            const auto* pExtInfo = static_cast<const VkAttachmentDescriptionStencilLayout*>(pNext);
            stencilInitialLayout = pExtInfo->stencilInitialLayout;
            stencilFinalLayout = pExtInfo->stencilFinalLayout;

            break;
        }
        default:
            // Skip any unknown extension structures.
            break;
        }

        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
SubpassDependency::SubpassDependency()
    :
    srcSubpass      (0),
    dstSubpass      (0),
    srcStageMask    (VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM),
    dstStageMask    (VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM),
    srcAccessMask   (VK_ACCESS_FLAG_BITS_MAX_ENUM),
    dstAccessMask   (VK_ACCESS_FLAG_BITS_MAX_ENUM),
    dependencyFlags (0),
    viewOffset      (0)
{
}

// =====================================================================================================================
void SubpassDependency::Init(
    uint32_t                        subpassDepIndex,
    const VkSubpassDependency&      subpassDep,
    const RenderPassExtCreateInfo&  renderPassExt)
{
    srcSubpass      = subpassDep.srcSubpass;
    dstSubpass      = subpassDep.dstSubpass;
    srcStageMask    = subpassDep.srcStageMask;
    dstStageMask    = subpassDep.dstStageMask;
    srcAccessMask   = subpassDep.srcAccessMask;
    dstAccessMask   = subpassDep.dstAccessMask;
    dependencyFlags = subpassDep.dependencyFlags;
    viewOffset      = 0;

    // The multiview implementation broadcasts 3D primities by
    // issuing multiple draw calls (one per each view),
    // therefore all view-local dependencies are treated as view-global.
    if (renderPassExt.pMultiviewCreateInfo != nullptr)
    {
        if (renderPassExt.pMultiviewCreateInfo->dependencyCount > 0)
        {
            viewOffset = renderPassExt.pMultiviewCreateInfo->pViewOffsets[subpassDepIndex];
        }
    }
}

// =====================================================================================================================
void SubpassDependency::Init(
    uint32_t                        subpassDepIndex,
    const VkSubpassDependency2&     subpassDep,
    const RenderPassExtCreateInfo&  renderPassExt)
{
    srcSubpass      = subpassDep.srcSubpass;
    dstSubpass      = subpassDep.dstSubpass;
    srcStageMask    = subpassDep.srcStageMask;
    dstStageMask    = subpassDep.dstStageMask;
    srcAccessMask   = subpassDep.srcAccessMask;
    dstAccessMask   = subpassDep.dstAccessMask;
    dependencyFlags = subpassDep.dependencyFlags;
    viewOffset      = subpassDep.viewOffset;

    EXTRACT_VK_STRUCTURES_0(
        barrier,
        MemoryBarrier2KHR,
        static_cast<const VkMemoryBarrier2KHR*>(subpassDep.pNext),
        MEMORY_BARRIER_2_KHR);

    if (pMemoryBarrier2KHR != nullptr)
    {
        srcStageMask  = pMemoryBarrier2KHR->srcStageMask;
        srcAccessMask = pMemoryBarrier2KHR->srcAccessMask;
        dstStageMask  = pMemoryBarrier2KHR->dstStageMask;
        dstAccessMask = pMemoryBarrier2KHR->dstAccessMask;
    }

    // The multiview implementation broadcasts 3D primities by
    // issuing multiple draw calls (one per each view),
    // therefore all view-local dependencies are treated as view-global.
    if (renderPassExt.pMultiviewCreateInfo != nullptr)
    {
        if (renderPassExt.pMultiviewCreateInfo->dependencyCount > 0)
        {
            viewOffset = renderPassExt.pMultiviewCreateInfo->pViewOffsets[subpassDepIndex];
        }
        else
        {
            viewOffset = 0;
        }
    }
}

// =====================================================================================================================
SubpassDescription::SubpassDescription()
    :
    flags                   (0),
    pipelineBindPoint       (VK_PIPELINE_BIND_POINT_MAX_ENUM),
    viewMask                (0),
    inputAttachmentCount    (0),
    pInputAttachments       (nullptr),
    colorAttachmentCount    (0),
    pColorAttachments       (nullptr),
    pResolveAttachments     (nullptr),
    preserveAttachmentCount (0),
    pPreserveAttachments    (nullptr),
    depthResolveMode        (VK_RESOLVE_MODE_NONE),
    stencilResolveMode      (VK_RESOLVE_MODE_NONE),
    hash                    (0ull)
{
}

// =====================================================================================================================
template <typename SubpassDescriptionType>
static size_t GetSubpassDescriptionBaseMemorySize(const SubpassDescriptionType& subpassDesc)
{
    size_t subpassMemorySize = 0;

    subpassMemorySize += subpassDesc.inputAttachmentCount * sizeof(AttachmentReference);
    subpassMemorySize += subpassDesc.colorAttachmentCount * sizeof(AttachmentReference);

    if (subpassDesc.pResolveAttachments != nullptr)
    {
        subpassMemorySize += subpassDesc.colorAttachmentCount * sizeof(AttachmentReference);
    }
    subpassMemorySize += subpassDesc.preserveAttachmentCount * sizeof(uint32_t);

    return subpassMemorySize;
}

// =====================================================================================================================
template <typename SubpassDescriptionType>
static void InitSubpassDescription(
    uint32_t                        subpassIndex,
    const SubpassDescriptionType&   subpassDesc,
    const RenderPassExtCreateInfo&  renderPassExt,
    const AttachmentDescription*    pAttachments,
    uint32_t                        attachmentCount,
    void*                           pMemoryPtr,
    size_t                          memorySize,
    SubpassDescription*             outDesc)
{
    void* nextPtr = pMemoryPtr;

    outDesc->flags             = subpassDesc.flags;
    outDesc->pipelineBindPoint = subpassDesc.pipelineBindPoint;

    // Copy input attachments references
    outDesc->inputAttachmentCount = subpassDesc.inputAttachmentCount;
    outDesc->pInputAttachments    = static_cast<AttachmentReference*>(nextPtr);

    for (uint32_t attachIndex = 0; attachIndex < subpassDesc.inputAttachmentCount; ++attachIndex)
    {
        outDesc->pInputAttachments[attachIndex].Init(subpassDesc.pInputAttachments[attachIndex]);
    }

    nextPtr = Util::VoidPtrInc(nextPtr, subpassDesc.inputAttachmentCount * sizeof(AttachmentReference));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    // Copy color attachments references
    outDesc->colorAttachmentCount = subpassDesc.colorAttachmentCount;
    outDesc->pColorAttachments    = reinterpret_cast<AttachmentReference*>(nextPtr);

    for (uint32_t attachIndex = 0; attachIndex < subpassDesc.colorAttachmentCount; ++attachIndex)
    {
        outDesc->pColorAttachments[attachIndex].Init(subpassDesc.pColorAttachments[attachIndex]);
    }

    nextPtr = Util::VoidPtrInc(nextPtr, subpassDesc.colorAttachmentCount * sizeof(AttachmentReference));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    // Copy resolve attachments references
    if (subpassDesc.pResolveAttachments != nullptr)
    {
        outDesc->pResolveAttachments = reinterpret_cast<AttachmentReference*>(nextPtr);

        for (uint32_t attachIndex = 0; attachIndex < subpassDesc.colorAttachmentCount; ++attachIndex)
        {
            outDesc->pResolveAttachments[attachIndex].Init(subpassDesc.pResolveAttachments[attachIndex]);
        }

        nextPtr = Util::VoidPtrInc(nextPtr, subpassDesc.colorAttachmentCount * sizeof(AttachmentReference));
        VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);
    }

    // Copy depth stencil attachment reference
    if (subpassDesc.pDepthStencilAttachment != nullptr)
    {
        outDesc->depthStencilAttachment.Init(*subpassDesc.pDepthStencilAttachment);
    }

    // Copy preserve attachments indices
    outDesc->preserveAttachmentCount = subpassDesc.preserveAttachmentCount;
    outDesc->pPreserveAttachments    = reinterpret_cast<uint32_t*>(nextPtr);

    if (subpassDesc.preserveAttachmentCount > 0)
    {
        memcpy(
            outDesc->pPreserveAttachments,
            subpassDesc.pPreserveAttachments,
            subpassDesc.preserveAttachmentCount * sizeof(uint32_t));
    }

    nextPtr = Util::VoidPtrInc(nextPtr, subpassDesc.preserveAttachmentCount * sizeof(uint32_t));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    // Even if pMultiviewCreateInfo == null, outDesc->viewMask was already handled in the calling function
    if (renderPassExt.pMultiviewCreateInfo != nullptr)
    {
        if (renderPassExt.pMultiviewCreateInfo->subpassCount > 0)
        {
            outDesc->viewMask = renderPassExt.pMultiviewCreateInfo->pViewMasks[subpassIndex];
        }
        else
        {
            outDesc->viewMask = 0;
        }
    }

    // Calculate the color and depth sample counts
    outDesc->subpassSampleCount.colorCount = 0;
    outDesc->subpassSampleCount.depthCount = 0;

    for (uint32_t subpassAttachIdx = 0; subpassAttachIdx < subpassDesc.colorAttachmentCount; ++subpassAttachIdx)
    {
        uint32_t subpassColorAttachment = outDesc->pColorAttachments[subpassAttachIdx].attachment;

        if (subpassColorAttachment != VK_ATTACHMENT_UNUSED && subpassColorAttachment < attachmentCount)
        {
            outDesc->subpassSampleCount.colorCount = pAttachments[subpassColorAttachment].samples != 0 ?
                pAttachments[subpassColorAttachment].samples : 1;

            // All sample counts within the subpass must match. We can exist as soon
            // as we've calculated the sample count for the first attachment we find.
            break;
        }
    }

    uint32_t subpassDepthAttachment = outDesc->depthStencilAttachment.attachment;

    if (subpassDepthAttachment != VK_ATTACHMENT_UNUSED && subpassDepthAttachment < attachmentCount)
    {
        outDesc->subpassSampleCount.depthCount = pAttachments[subpassDepthAttachment].samples != 0 ?
            pAttachments[subpassDepthAttachment].samples : 1;
    }
}

// =====================================================================================================================
void SubpassDescription::Init(
    uint32_t                        subpassIndex,
    const VkSubpassDescription&     subpassDesc,
    const RenderPassExtCreateInfo&  renderPassExt,
    const AttachmentDescription*    pAttachments,
    uint32_t                        attachmentCount,
    void*                           pMemoryPtr,
    size_t                          memorySize)
{
    InitSubpassDescription<VkSubpassDescription>(
        subpassIndex,
        subpassDesc,
        renderPassExt,
        pAttachments,
        attachmentCount,
        pMemoryPtr,
        memorySize,
        this);
}

// =====================================================================================================================
void SubpassDescription::Init(
    uint32_t                        subpassIndex,
    const VkSubpassDescription2&    subpassDesc,
    const RenderPassExtCreateInfo&  renderPassExt,
    const AttachmentDescription*    pAttachments,
    uint32_t                        attachmentCount,
    void*                           pMemoryPtr,
    size_t                          memorySize)
{
    viewMask = subpassDesc.viewMask;

    const void* pNext = subpassDesc.pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE:
        {
            const auto* pExtInfo = static_cast<const VkSubpassDescriptionDepthStencilResolve*>(pNext);
            depthResolveMode = pExtInfo->depthResolveMode;
            stencilResolveMode = pExtInfo->stencilResolveMode;

            if (pExtInfo->pDepthStencilResolveAttachment != nullptr)
            {
                depthStencilResolveAttachment.Init(
                    *(pExtInfo->pDepthStencilResolveAttachment));
            }

            break;
        }
        case VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR:
        {
            const auto* pExtInfo = static_cast<const VkFragmentShadingRateAttachmentInfoKHR*>(pNext);

            if (pExtInfo->pFragmentShadingRateAttachment != nullptr)
            {
                fragmentShadingRateAttachment.Init(
                    *pExtInfo->pFragmentShadingRateAttachment);
            }

            break;
        }
        default:
            break;
        }

        pNext = pHeader->pNext;
    }

    InitSubpassDescription<VkSubpassDescription2>(
        subpassIndex,
        subpassDesc,
        renderPassExt,
        pAttachments,
        attachmentCount,
        pMemoryPtr,
        memorySize,
        this);
}

// =====================================================================================================================
RenderPassCreateInfo::RenderPassCreateInfo()
    :
    flags                   (0),
    attachmentCount         (0),
    pAttachments            (nullptr),
    subpassCount            (0),
    pSubpasses              (nullptr),
    dependencyCount         (0),
    pDependencies           (nullptr),
    correlatedViewMaskCount (0),
    pCorrelatedViewMasks    (nullptr),
    hash                    (0)
{
}

// =====================================================================================================================
template<typename RenderPassCreateInfoType>
static size_t GetRenderPassCreateInfoRequiredMemorySize(
    const RenderPassCreateInfoType*     pCreateInfo,
    const RenderPassExtCreateInfo&      renderPassExt)
{
    size_t createInfoSize = 0;

    if (renderPassExt.pMultiviewCreateInfo != nullptr)
    {
        createInfoSize += renderPassExt.pMultiviewCreateInfo->correlationMaskCount * sizeof(uint32_t);
    }
    else if (std::is_same<RenderPassCreateInfoType, VkRenderPassCreateInfo2>::value)
    {
        auto pCreateInfo2 = reinterpret_cast<const VkRenderPassCreateInfo2*>(pCreateInfo);

        createInfoSize += pCreateInfo2->correlatedViewMaskCount * sizeof(uint32_t);
    }

    createInfoSize += pCreateInfo->attachmentCount * sizeof(AttachmentDescription);
    // Subpasses need to be aligned
    createInfoSize = Util::Pow2Align(createInfoSize, alignof(SubpassDescription));
    createInfoSize += pCreateInfo->subpassCount * sizeof(SubpassDescription);
    createInfoSize += pCreateInfo->dependencyCount * sizeof(SubpassDependency);

    for (uint32_t subpassIndex = 0; subpassIndex < pCreateInfo->subpassCount; ++subpassIndex)
    {
        const auto& subpassDesc = pCreateInfo->pSubpasses[subpassIndex];

        createInfoSize += GetSubpassDescriptionBaseMemorySize(subpassDesc);
    }

    return createInfoSize;
}

// =====================================================================================================================
// Check if forcing lateZ is needed
template <typename RenderPassCreateInfoType>
static bool CheckIfForceLateZNeeded(
    const RenderPassCreateInfoType* pCreateInfo)
{
    // When there is a valid "feedback loop" in renderpass, lateZ needs to be enabled
    // In Vulkan a "feedback loop" is described as a subpass where there is at least
    // one input attachment that is also a color or depth/stencil attachment
    // Feedback loops are allowed and their behavior is well defined under certain conditions.
    // When there is a feedback loop it is possible for the shaders to read
    // the contents of the color and depth or stencil attachments
    // from the shader during draw. Because of that possibility you have to use late-z

    bool found = false;
    for (uint32 i = 0; (i < pCreateInfo->subpassCount) && (found == false); i++)
    {
        if (pCreateInfo->pSubpasses[i].pDepthStencilAttachment != nullptr)
        {
            for (uint32 j = 0; (j < pCreateInfo->pSubpasses[i].inputAttachmentCount) && (found == false); j++)
            {
                for (uint32 k = 0; (k < pCreateInfo->pSubpasses[i].colorAttachmentCount) && (found == false); k++)
                {
                    if (pCreateInfo->pSubpasses[i].pInputAttachments[j].attachment ==
                        pCreateInfo->pSubpasses[i].pColorAttachments[k].attachment)
                    {
                        found = true;
                    }
                }
            }
        }
    }

    return found;
}

// =====================================================================================================================
// Creates a render pass
template <typename RenderPassCreateInfoType>
static void InitRenderPassCreateInfo(
    const RenderPassCreateInfoType*     pCreateInfo,
    const RenderPassExtCreateInfo&      renderPassExt,
    void*                               pMemoryPtr,
    size_t                              memorySize,
    RenderPassCreateInfo*               pOutRenderPassInfo)
{
    void* nextPtr = pMemoryPtr;

    pOutRenderPassInfo->flags = pCreateInfo->flags;

    // The multiview implementation does not exploit any coherence between views.
    if (renderPassExt.pMultiviewCreateInfo != nullptr)
    {
        pOutRenderPassInfo->correlatedViewMaskCount = renderPassExt.pMultiviewCreateInfo->correlationMaskCount;
        pOutRenderPassInfo->pCorrelatedViewMasks    = static_cast<uint32_t*>(nextPtr);

        memcpy(
            pOutRenderPassInfo->pCorrelatedViewMasks,
            renderPassExt.pMultiviewCreateInfo->pCorrelationMasks,
            pOutRenderPassInfo->correlatedViewMaskCount * sizeof(uint32_t));
    }

    nextPtr = Util::VoidPtrInc(nextPtr, pOutRenderPassInfo->correlatedViewMaskCount * sizeof(uint32_t));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    pOutRenderPassInfo->attachmentCount  = pCreateInfo->attachmentCount;
    pOutRenderPassInfo->pAttachments     = static_cast<AttachmentDescription*>(nextPtr);

    for (uint32_t attachIndex = 0; attachIndex < pCreateInfo->attachmentCount; ++attachIndex)
    {
        pOutRenderPassInfo->pAttachments[attachIndex].Init(pCreateInfo->pAttachments[attachIndex]);
    }

    nextPtr = Util::VoidPtrInc(nextPtr, pCreateInfo->attachmentCount * sizeof(AttachmentDescription));
    // Struct needs to be aligned
    nextPtr = Util::VoidPtrAlign(nextPtr, alignof(SubpassDescription));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    pOutRenderPassInfo->subpassCount = pCreateInfo->subpassCount;
    pOutRenderPassInfo->pSubpasses   = static_cast<SubpassDescription*>(nextPtr);

    nextPtr = Util::VoidPtrInc(nextPtr, pCreateInfo->subpassCount * sizeof(SubpassDescription));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    void*   subpassDescMemory        = nextPtr;
    size_t  subpassDescAllMemorySize = 0;

    for (uint32_t subpassIndex = 0; subpassIndex < pCreateInfo->subpassCount; ++subpassIndex)
    {
        const auto&    subpassDesc           = pCreateInfo->pSubpasses[subpassIndex];
        const size_t   subpassDescMemorySize = GetSubpassDescriptionBaseMemorySize(subpassDesc);

        VK_PLACEMENT_NEW(&pOutRenderPassInfo->pSubpasses[subpassIndex]) SubpassDescription();

        pOutRenderPassInfo->pSubpasses[subpassIndex].Init(
            subpassIndex,
            subpassDesc,
            renderPassExt,
            pOutRenderPassInfo->pAttachments,
            pOutRenderPassInfo->attachmentCount,
            subpassDescMemory,
            subpassDescMemorySize);

        subpassDescMemory = Util::VoidPtrInc(subpassDescMemory, subpassDescMemorySize);

        subpassDescAllMemorySize += subpassDescMemorySize;
    }

    nextPtr = Util::VoidPtrInc(nextPtr, subpassDescAllMemorySize);
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    pOutRenderPassInfo->dependencyCount = pCreateInfo->dependencyCount;
    pOutRenderPassInfo->pDependencies   = static_cast<SubpassDependency*>(nextPtr);

    for (uint32_t depIndex = 0; depIndex < pCreateInfo->dependencyCount; ++depIndex)
    {
        pOutRenderPassInfo->pDependencies[depIndex].Init(
            depIndex,
            pCreateInfo->pDependencies[depIndex],
            renderPassExt);
    }

    pOutRenderPassInfo->needForceLateZ = CheckIfForceLateZNeeded(pCreateInfo);

    nextPtr = Util::VoidPtrInc(nextPtr, pCreateInfo->dependencyCount * sizeof(SubpassDependency));
    VK_ASSERT(Util::VoidPtrDiff(nextPtr, pMemoryPtr) <= memorySize);

    pOutRenderPassInfo->hash = GenerateRenderPassHash(pOutRenderPassInfo);
    for (uint32_t i = 0; i < pOutRenderPassInfo->subpassCount; i++)
    {
        pOutRenderPassInfo->pSubpasses[i].hash = GenerateSubpassHash(pOutRenderPassInfo, i);
    }
}

// =====================================================================================================================
void RenderPassCreateInfo::Init(
    const VkRenderPassCreateInfo*       pCreateInfo,
    const RenderPassExtCreateInfo&      renderPassExt,
    void*                               pMemoryPtr,
    size_t                              memorySize)
{
    InitRenderPassCreateInfo<VkRenderPassCreateInfo>(
        pCreateInfo,
        renderPassExt,
        pMemoryPtr,
        memorySize,
        this);
}

// =====================================================================================================================
void RenderPassCreateInfo::Init(
    const VkRenderPassCreateInfo2*      pCreateInfo,
    const RenderPassExtCreateInfo&      renderPassExt,
    void*                               pMemoryPtr,
    size_t                              memorySize)
{
    // The multiview implementation does not exploit any coherence between views.
    if (renderPassExt.pMultiviewCreateInfo == nullptr)
    {
        correlatedViewMaskCount = pCreateInfo->correlatedViewMaskCount;
        pCorrelatedViewMasks    = static_cast<uint32_t*>(pMemoryPtr);

        memcpy(
            pCorrelatedViewMasks,
            pCreateInfo->pCorrelatedViewMasks,
            pCreateInfo->correlatedViewMaskCount * sizeof(uint32_t));
    }

    InitRenderPassCreateInfo<VkRenderPassCreateInfo2>(
        pCreateInfo,
        renderPassExt,
        pMemoryPtr,
        memorySize,
        this);
}

// =====================================================================================================================
RenderPass::RenderPass(
    const RenderPassCreateInfo*     pCreateInfo,
    const RenderPassExecuteInfo*    pExecuteInfo)
    :
    m_createInfo    (*pCreateInfo),
    m_pExecuteInfo  (pExecuteInfo)
{
}

// =====================================================================================================================
// Creates a render pass
template <typename RenderPassCreateInfoType>
static VkResult CreateRenderPass(
    Device*                             pDevice,
    const RenderPassCreateInfoType*     pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkRenderPass*                       pOutRenderPass)
{
    VkResult result  = VK_SUCCESS;
    void*    pMemory = nullptr;

    utils::TempMemArena buildArena(pAllocator, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

    RenderPassExtCreateInfo renderPassExt;

    const void* pNext = pCreateInfo->pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO:
        {
            const auto* pExtInfo = static_cast<const VkRenderPassMultiviewCreateInfo*>(pNext);
            VK_ASSERT((pExtInfo->subpassCount) == 0 ||
                     (pExtInfo->subpassCount == pCreateInfo->subpassCount));
            VK_ASSERT((pExtInfo->dependencyCount == 0) ||
                     (pExtInfo->dependencyCount == pCreateInfo->dependencyCount));
            renderPassExt.pMultiviewCreateInfo = pExtInfo;

            break;
        }
        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }

    const size_t apiSize        = sizeof(RenderPass);
    const size_t infoMemorySize = GetRenderPassCreateInfoRequiredMemorySize<RenderPassCreateInfoType>(pCreateInfo, renderPassExt);

    const size_t memorySize = apiSize + infoMemorySize;

    pMemory = pDevice->AllocApiObject(pAllocator, memorySize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    RenderPassCreateInfo renderPassInfo;

    void* pMemoryInfo = Util::VoidPtrInc(pMemory, apiSize);

    renderPassInfo.Init(
        pCreateInfo,
        renderPassExt,
        pMemoryInfo,
        infoMemorySize);

    RenderPassExecuteInfo* pExecuteInfo = nullptr;

    RenderPassBuilder builder(pDevice, &buildArena);

    result = builder.Build(
        &renderPassInfo,
        pAllocator,
        &pExecuteInfo);

    if (result != VK_SUCCESS)
    {
        if (pExecuteInfo != nullptr)
        {
            pExecuteInfo->~RenderPassExecuteInfo();
            pAllocator->pfnFree(pAllocator->pUserData, pExecuteInfo);
        }

        if (pMemory != nullptr)
        {
            pDevice->FreeApiObject(pAllocator, pMemory);
        }

        return result;
    }

    VK_PLACEMENT_NEW(pMemory) RenderPass(&renderPassInfo, pExecuteInfo);

    *pOutRenderPass = RenderPass::HandleFromVoidPointer(pMemory);

    return result;
}

// =====================================================================================================================
VkResult RenderPass::Create(
    Device*                             pDevice,
    const VkRenderPassCreateInfo*       pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkRenderPass*                       pRenderPass)
{
    return CreateRenderPass<VkRenderPassCreateInfo>(
        pDevice,
        pCreateInfo,
        pAllocator,
        pRenderPass);
}

// =====================================================================================================================
VkResult RenderPass::Create(
    Device*                             pDevice,
    const VkRenderPassCreateInfo2*      pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkRenderPass*                       pRenderPass)
{
    return CreateRenderPass<VkRenderPassCreateInfo2>(
        pDevice,
        pCreateInfo,
        pAllocator,
        pRenderPass);
}

// =====================================================================================================================
// Returns the output format of a particular color attachment in a particular subpass
VkFormat RenderPass::GetColorAttachmentFormat(
    uint32_t subpassIndex,
    uint32_t colorTarget
    ) const
{
    const SubpassDescription& subPass = m_createInfo.pSubpasses[subpassIndex];
    const uint32_t attachIndex        = subPass.pColorAttachments[colorTarget].attachment;

    VkFormat format;

    if (subPass.colorAttachmentCount > 0 && attachIndex != VK_ATTACHMENT_UNUSED)
    {
        format = m_createInfo.pAttachments[attachIndex].format;
    }
    else
    {
        format = VK_FORMAT_UNDEFINED;
    }

    return format;
}

// =====================================================================================================================
// Returns the depth stencil format in a particular subpass
VkFormat RenderPass::GetDepthStencilAttachmentFormat(
    uint32_t subpassIndex
    ) const
{
    VkFormat format;

    const SubpassDescription& subpass = m_createInfo.pSubpasses[subpassIndex];

    if (subpass.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED)
    {
        const uint32_t attachIndex = subpass.depthStencilAttachment.attachment;

        format = m_createInfo.pAttachments[attachIndex].format;
    }
    else
    {
        format = VK_FORMAT_UNDEFINED;
    }

    return format;
}

// =====================================================================================================================
// Returns the output sample count of a particular color attachment in a particular subpass
uint32_t RenderPass::GetColorAttachmentSamples(
    uint32_t subpassIndex,
    uint32_t colorTarget
    ) const
{
    const SubpassDescription& subPass = m_createInfo.pSubpasses[subpassIndex];
    const uint32_t attachIndex        = subPass.pColorAttachments[colorTarget].attachment;

    uint32_t samples;

    if (attachIndex != VK_ATTACHMENT_UNUSED)
    {
        samples = m_createInfo.pAttachments[attachIndex].samples;
    }
    else
    {
        samples = 1;
    }

    return samples;
}

// =====================================================================================================================
// Returns the depth stencil attachment sample count in a particular subpass
uint32_t RenderPass::GetDepthStencilAttachmentSamples(
    uint32_t subPassIndex
    ) const
{
    uint32_t samples;

    const SubpassDescription& subPass = m_createInfo.pSubpasses[subPassIndex];

    if (subPass.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED)
    {
        const uint32_t attachIndex = subPass.depthStencilAttachment.attachment;

        samples = m_createInfo.pAttachments[attachIndex].samples;
    }
    else
    {
        samples = 1;
    }

    return samples;
}

// =====================================================================================================================
// Return the subPass's color attachment count
uint32_t RenderPass::GetSubpassColorReferenceCount(
    uint32_t subpassIndex
    ) const
{
    return m_createInfo.pSubpasses[subpassIndex].colorAttachmentCount;
}

// =====================================================================================================================
const AttachmentReference& RenderPass::GetSubpassColorReference(
    uint32_t subpass,
    uint32_t index
    ) const
{
    return m_createInfo.pSubpasses[subpass].pColorAttachments[index];
}

// =====================================================================================================================
const AttachmentReference& RenderPass::GetSubpassDepthStencilReference(
    uint32_t subpass
    ) const
{
    return m_createInfo.pSubpasses[subpass].depthStencilAttachment;
}

// =====================================================================================================================
const AttachmentDescription& RenderPass::GetAttachmentDesc(
    uint32_t attachmentIndex
    ) const
{
    VK_ASSERT(attachmentIndex < m_createInfo.attachmentCount);

    return m_createInfo.pAttachments[attachmentIndex];
}

// =====================================================================================================================
// Destroys a render pass object
VkResult RenderPass::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    pAllocator->pfnFree(pAllocator->pUserData, const_cast<RenderPassExecuteInfo*>(m_pExecuteInfo));

    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
}

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(
    VkDevice                                    device,
    VkRenderPass                                renderPass,
    const VkAllocationCallbacks*                pAllocator)
{
    if (renderPass != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        RenderPass::ObjectFromHandle(renderPass)->Destroy(pDevice, pAllocCB);
    }
}
} // namespace entry

} // namespace vk
