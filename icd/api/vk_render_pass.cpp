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

#include <cmath>

#include "include/vk_conv.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_object.h"
#include "include/vk_render_pass.h"
#include "include/vk_framebuffer.h"
#include "include/vk_cmdbuffer.h"

#include "renderpass/renderpass_builder.h"
#include "renderpass/renderpass_logger.h"

#include "palVectorImpl.h"
#include "palMd5.h"

namespace vk
{

static uint64_t GenerateRenderPassHash(const VkRenderPassCreateInfo* pIn);

struct RenderPassExtCreateInfo
{
};

// =====================================================================================================================
static void ConvertRenderPassCreateInfo(
    const VkRenderPassCreateInfo*  pIn,
    const RenderPassExtCreateInfo& renderPassExtCreateInfo,
    RenderPassCreateInfo*          pInfo,
    VkAttachmentReference* const   pSubpassColorAttachments)
{
    // !!! IMPORTANT !!!
    //
    // Please be conscious that any changes to this function or other information derived from the render pass
    // create info should also be reflected with updates to the GenerateRenderPassHash() function.
    //
    // !!! IMPORTANT !!!

    memcpy(pInfo->pAttachments, pIn->pAttachments, pInfo->attachmentCount * sizeof(VkAttachmentDescription));

    VkAttachmentReference* pNextColorAttachments = pSubpassColorAttachments;

    for (uint32_t subIter = 0; subIter < pInfo->subpassCount; ++subIter)
    {
        pInfo->pSubpasses[subIter].pColorAttachments = pNextColorAttachments;

        VK_PLACEMENT_NEW(&pInfo->pSubpasses[subIter]) RenderSubPass(subIter, *pIn);

        pNextColorAttachments += pIn->pSubpasses[subIter].colorAttachmentCount;

        pInfo->pSubpasses[subIter].viewMask = 0;
    }

    // pInfo->pSubpassSampleCounts will contain the color and depth sample counts per subpass.
    memset(pInfo->pSubpassSampleCounts, 0, pInfo->subpassCount * sizeof(SubpassSampleCounts));

    // Calculate the color and depth sample counts.
    uint32_t subpassColorSampleCount = 0;
    uint32_t subpassDepthSampleCount = 0;
    uint32_t subpassMaxSampleCount   = 0;

    for (uint32_t subpassIndex = 0; subpassIndex < pInfo->subpassCount; subpassIndex++)
    {
        uint32_t colorAttachmentCount = pInfo->pSubpasses[subpassIndex].colorAttachmentCount;

        for (uint32_t subpassAttachmentIdx = 0; subpassAttachmentIdx < colorAttachmentCount; subpassAttachmentIdx++)
        {
            uint32_t subpassColorAttachment =
                pInfo->pSubpasses[subpassIndex].pColorAttachments[subpassAttachmentIdx].attachment;

            if (subpassColorAttachment != VK_ATTACHMENT_UNUSED)
            {
                for (uint32_t attachIdx = 0; attachIdx < pInfo->attachmentCount; ++attachIdx)
                {
                    if (attachIdx == subpassColorAttachment)
                    {
                        subpassColorSampleCount = pInfo->pAttachments[attachIdx].samples != 0 ?
                            pInfo->pAttachments[attachIdx].samples : 1;

                        // All sample counts within the subpass must match. We can exist as soon
                        // as we've calculated the sample count for the first attachment we find.
                        break;
                    }
                }
            }
        }

        // In case there are no color attachments, check depth.
        uint32_t subpassDepthAttachment = pInfo->pSubpasses[subpassIndex].depthStencilAttachment.attachment;

        if (subpassDepthAttachment != VK_ATTACHMENT_UNUSED)
        {
            for (uint32_t attachIdx = 0; attachIdx < pInfo->attachmentCount; ++attachIdx)
            {
                if (attachIdx == subpassDepthAttachment)
                {
                    subpassDepthSampleCount = pInfo->pAttachments[attachIdx].samples != 0 ?
                        pInfo->pAttachments[attachIdx].samples : 1;

                    // There is only one depth attachment. Exit the loop when we find it.
                    break;
                }
            }
        }

        pInfo->pSubpassSampleCounts[subpassIndex].colorCount = subpassColorSampleCount;
        pInfo->pSubpassSampleCounts[subpassIndex].depthCount = subpassDepthSampleCount;
    }

    pInfo->hash = GenerateRenderPassHash(pIn);
}

#define RPHashStructField(x) \
    Util::Md5::Update( \
        pContext, \
        reinterpret_cast<const uint8_t*>(&x), \
        sizeof(x));

// =====================================================================================================================
static void GenerateHashFromSubPassDesc(
    Util::Md5::Context*         pContext,
    const VkSubpassDescription& desc)
{
    uint32_t inputCount    = desc.inputAttachmentCount;
    uint32_t colorCount    = desc.colorAttachmentCount;
    uint32_t preserveCount = desc.preserveAttachmentCount;

    RPHashStructField(desc.flags);
    RPHashStructField(desc.pipelineBindPoint);
    RPHashStructField(desc.inputAttachmentCount);
    RPHashStructField(desc.colorAttachmentCount);
    RPHashStructField(desc.preserveAttachmentCount);

    if (inputCount > 0)
    {
        Util::Md5::Update(
            pContext,
            reinterpret_cast<const uint8_t*>(desc.pInputAttachments),
            inputCount * sizeof(desc.pInputAttachments[0]));
    }
    if (colorCount > 0)
    {
        Util::Md5::Update(
            pContext,
            reinterpret_cast<const uint8_t*>(desc.pColorAttachments),
            colorCount * sizeof(desc.pColorAttachments[0]));
    }
    if (preserveCount > 0)
    {
        Util::Md5::Update(
            pContext,
            reinterpret_cast<const uint8_t*>(desc.pPreserveAttachments),
            preserveCount * sizeof(desc.pPreserveAttachments[0]));
    }
    if ((desc.pResolveAttachments != nullptr) && (colorCount > 0))
    {
        Util::Md5::Update(
            pContext,
            reinterpret_cast<const uint8_t*>(desc.pResolveAttachments),
            colorCount * sizeof(desc.pResolveAttachments[0]));
    }
    if (desc.pDepthStencilAttachment != nullptr)
    {
        Util::Md5::Update(
            pContext,
            reinterpret_cast<const uint8_t*>(desc.pDepthStencilAttachment),
            sizeof(desc.pDepthStencilAttachment[0]));
    }
}

// =====================================================================================================================
void GenerateHashFromCreateInfo(
    Util::Md5::Context*           pContext,
    const VkRenderPassCreateInfo& info)
{
    VkRenderPassCreateInfo copy = info;

    copy.pAttachments  = nullptr;
    copy.pSubpasses    = nullptr;
    copy.pDependencies = nullptr;

    Util::Md5::Update(
        pContext,
        reinterpret_cast<const uint8_t*>(&copy),
        sizeof(copy));
    Util::Md5::Update(
        pContext,
        reinterpret_cast<const uint8_t*>(info.pAttachments),
        info.attachmentCount * sizeof(VkAttachmentDescription));
    Util::Md5::Update(
        pContext,
        reinterpret_cast<const uint8_t*>(info.pDependencies),
        info.dependencyCount * sizeof(VkSubpassDependency));

    for (uint32_t i = 0; i < info.subpassCount; ++i)
    {
        GenerateHashFromSubPassDesc(pContext, info.pSubpasses[i]);
    }
}

// =====================================================================================================================
uint64_t GenerateRenderPassHash(
    const VkRenderPassCreateInfo* pIn)
{
    if (pIn == nullptr)
    {
        return 0;
    }

    Util::Md5::Context context = {};

    Util::Md5::Init(&context);

    GenerateHashFromCreateInfo(&context, *pIn);

    Util::Md5::Hash hash = {};

    Util::Md5::Final(&context, &hash);

    return Util::Md5::Compact64(&hash);
}

// =====================================================================================================================
// Creates a render pass
VkResult RenderPass::Create(
    Device*                       pDevice,
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*  pAllocator,
    VkRenderPass*                 pRenderPass)
{
    VkResult result = VK_SUCCESS;

    utils::TempMemArena buildArena(pAllocator, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);

    RenderPassCreateInfo info = {};
    void* pMemory = nullptr;
    const auto& settings = pDevice->GetRuntimeSettings();

    const VkRenderPassCreateInfo* pRenderPassHeader = nullptr;
    RenderPassExtCreateInfo renderPassExt = {};

    union
    {
        const VkStructHeader*                                         pHeader;
        const VkRenderPassCreateInfo*                                 pRenderPassCreateInfo;
    };

    for (pRenderPassCreateInfo = pCreateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (static_cast<uint32_t>(pHeader->sType))
        {
        case VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO:
            {
                info.subpassCount    = pCreateInfo->subpassCount;
                info.attachmentCount = pCreateInfo->attachmentCount;
                pRenderPassHeader    = pRenderPassCreateInfo;
            }
            break;

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    if (pRenderPassHeader == nullptr)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
    }

    if (result == VK_SUCCESS)
    {
        const size_t apiSize               = sizeof(RenderPass);
        const size_t attachmentSize        = info.attachmentCount * sizeof(VkAttachmentDescription);
        const size_t subpassSize           = info.subpassCount * sizeof(RenderSubPass);
        const size_t sampleCountsSize      = info.subpassCount * sizeof(SubpassSampleCounts);
        size_t       subpassAttachmentSize = 0;

        for (uint32_t i = 0; i < info.subpassCount; ++i)
        {
            subpassAttachmentSize += pCreateInfo->pSubpasses[i].colorAttachmentCount * sizeof(VkAttachmentReference);
        }

        const size_t objSize = apiSize
                             + attachmentSize
                             + subpassSize
                             + subpassAttachmentSize
                             + sampleCountsSize;

        pMemory = pDevice->AllocApiObject(objSize, pAllocator);

        if (pMemory != nullptr)
        {
            void* nextPtr = Util::VoidPtrInc(pMemory, apiSize);

            info.pAttachments = static_cast<VkAttachmentDescription*>(nextPtr);
            nextPtr = Util::VoidPtrInc(nextPtr, attachmentSize);

            info.pSubpasses = static_cast<RenderSubPass*>(nextPtr);
            nextPtr = Util::VoidPtrInc(nextPtr, subpassSize);

            VkAttachmentReference* pSubpassColorAttachments = static_cast<VkAttachmentReference*>(nextPtr);
            nextPtr = Util::VoidPtrInc(nextPtr, subpassAttachmentSize);

            info.pSubpassSampleCounts = static_cast<SubpassSampleCounts*>(nextPtr);

            ConvertRenderPassCreateInfo(
                pRenderPassHeader,
                renderPassExt,
                &info,
                pSubpassColorAttachments);
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    RenderPassExecuteInfo* pExecuteInfo = nullptr;
    RenderPassLogger*      pLogger      = nullptr;

#if ICD_LOG_RENDER_PASSES
    RenderPassLogger logger(&buildArena, pDevice);

    pLogger = &logger;
#endif

    if (result == VK_SUCCESS)
    {
        RenderPassLogBegin(pLogger, *pCreateInfo, info);

        RenderPassBuilder builder(pDevice, &buildArena, pLogger);

        result = builder.Build(*pCreateInfo,
                               info,
                               pAllocator,
                               &pExecuteInfo);
    }

    if (result == VK_SUCCESS)
    {
        RenderPassLogExecuteInfo(pLogger, pExecuteInfo);

        RenderPassLogEnd(pLogger);
    }

    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pMemory) RenderPass(info, pExecuteInfo);

        *pRenderPass = RenderPass::HandleFromVoidPointer(pMemory);
    }
    else
    {
        if (pExecuteInfo != nullptr)
        {
            pAllocator->pfnFree(pAllocator->pUserData, pExecuteInfo);
        }

        if (pMemory != nullptr)
        {
            pAllocator->pfnFree(pAllocator->pUserData, pMemory);
        }
    }

    return result;
}

// =====================================================================================================================
RenderPass::RenderPass(
    const RenderPassCreateInfo& info,
    RenderPassExecuteInfo*      pExecuteInfo)
    :
    m_createInfo(info),
    m_pExecuteInfo(pExecuteInfo)
{
}

// =====================================================================================================================
// Returns the output format of a particular color attachment in a particular subpass
VkFormat RenderPass::GetColorAttachmentFormat(
    uint32_t subpassIndex,
    uint32_t colorTarget
    ) const
{
    const RenderSubPass& subPass = m_createInfo.pSubpasses[subpassIndex];
    const uint32_t attachIndex   = subPass.pColorAttachments[colorTarget].attachment;

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

    const RenderSubPass& subpass = m_createInfo.pSubpasses[subpassIndex];

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
    const RenderSubPass& subPass = m_createInfo.pSubpasses[subpassIndex];
    const uint32_t attachIndex = subPass.pColorAttachments[colorTarget].attachment;

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

    const RenderSubPass& subPass = m_createInfo.pSubpasses[subPassIndex];

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
const VkAttachmentReference& RenderPass::GetSubpassColorReference(
    uint32_t subpass,
    uint32_t index
    ) const
{
    return m_createInfo.pSubpasses[subpass].pColorAttachments[index];
}

// =====================================================================================================================
const VkAttachmentReference& RenderPass::GetSubpassDepthStencilReference(
    uint32_t subpass
    ) const
{
    return m_createInfo.pSubpasses[subpass].depthStencilAttachment;
}

// =====================================================================================================================
const VkAttachmentDescription& RenderPass::GetAttachmentDesc(
    uint32_t attachmentIndex
    ) const
{
    VK_ASSERT(attachmentIndex < m_createInfo.attachmentCount);

    return m_createInfo.pAttachments[attachmentIndex];
}

// =====================================================================================================================
// Destroys a render pass object
VkResult RenderPass::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    pAllocator->pfnFree(pAllocator->pUserData, m_pExecuteInfo);

    // Call destructor
    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
RenderSubPass::RenderSubPass(
    uint32_t                      subPassIndex,
    const VkRenderPassCreateInfo& info)
{
    const VkSubpassDescription& desc = info.pSubpasses[subPassIndex];

    // Copy color attachment references

    colorAttachmentCount = desc.colorAttachmentCount;
    memcpy(pColorAttachments, desc.pColorAttachments, sizeof(VkAttachmentReference) * colorAttachmentCount);

    // Copy depth stencil attachment references
    if (desc.pDepthStencilAttachment != nullptr)
    {
        depthStencilAttachment = *desc.pDepthStencilAttachment;
    }
    else
    {
        depthStencilAttachment.attachment = VK_ATTACHMENT_UNUSED;
        depthStencilAttachment.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
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
        const Device*                pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        RenderPass::ObjectFromHandle(renderPass)->Destroy(pDevice, pAllocCB);
    }
}
} // namespace entry

} // namespace vk
