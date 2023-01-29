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

#include "include/vk_cmdbuffer.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_framebuffer.h"
#include "include/vk_image.h"
#include "include/vk_image_view.h"
#include "include/vk_instance.h"
#include "include/vk_render_pass.h"

#include "palColorTargetView.h"
#include "palDepthStencilView.h"
#include "palVectorImpl.h"

namespace vk
{

using namespace Util;

// =====================================================================================================================
// Finds SubresRanges defined by aspectMask, which are avaliable in the Attachment.
Vector<Pal::SubresRange, MaxRangePerAttachment, GenericAllocator>
Framebuffer::Attachment::FindSubresRanges(
    const VkImageAspectFlags aspectMask
    ) const
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Vector<Pal::SubresRange, MaxRangePerAttachment, GenericAllocator> subresRanges { nullptr };

    for (uint32_t i = 0; i < subresRangeCount; ++i)
    {
        const uint32_t attachmentPlane = subresRange[i].startSubres.plane;

        const bool   colorAvailable = (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) &&
                                      (attachmentPlane == 0);

        const bool   depthAvailable = (aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                                      (attachmentPlane == 0);

        const bool stencilAvailable = (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                                      (((attachmentPlane == 0) && (!pImage->HasDepth())) ||
                                       (attachmentPlane == 1));

        const bool     yuvAvailable = (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT);

        if (colorAvailable || depthAvailable || stencilAvailable || yuvAvailable)
        {
            VK_ASSERT(subresRanges.NumElements() < MaxRangePerAttachment);

            subresRanges.PushBack(subresRange[i]);
        }
    }

    return subresRanges;
}

// =====================================================================================================================
VkResult Framebuffer::Create(
    Device*                        pDevice,
    const VkFramebufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkFramebuffer*                 pFramebuffer)
{
    VkResult result = VK_SUCCESS;

    size_t apiSize = sizeof(Framebuffer);
    if (pCreateInfo->attachmentCount > 0)
    {
        // We need to add alignment for attachments.
        apiSize = GetAttachmentsOffset();
    }

    const size_t attachmentSize = sizeof(Attachment) * pCreateInfo->attachmentCount;
    const size_t objSize        = apiSize + attachmentSize;

    void* pSystemMem = pDevice->AllocApiObject(pAllocator, objSize);

    // On success, wrap it up in a Vulkan object and return.
    if (pSystemMem != nullptr)
    {
        Attachment* pAttachments = static_cast<Attachment*>(Util::VoidPtrInc(pSystemMem, apiSize));

        VK_PLACEMENT_NEW(pSystemMem) Framebuffer(*pCreateInfo, pAttachments, pDevice->GetRuntimeSettings());

        *pFramebuffer = Framebuffer::HandleFromVoidPointer(pSystemMem);
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return result;
}

// =====================================================================================================================
// Computes the dimensions of the given mip level in the image
static Pal::Extent3d ComputeLevelDimensions(
    Pal::Extent3d baseExtent,
    uint32_t      mipLevel)
{
    baseExtent.width  = Util::Max(baseExtent.width  >> mipLevel, static_cast<Pal::uint32>(1));
    baseExtent.height = Util::Max(baseExtent.height >> mipLevel, static_cast<Pal::uint32>(1));
    baseExtent.depth  = Util::Max(baseExtent.depth  >> mipLevel, static_cast<Pal::uint32>(1));

    return baseExtent;
}

// =====================================================================================================================
Framebuffer::Framebuffer(const VkFramebufferCreateInfo& info,
                         Attachment*                    pAttachments,
                         const RuntimeSettings&         runTimeSettings)
        : m_attachmentCount (info.attachmentCount),
          m_settings(runTimeSettings),
          m_flags(info.flags)
{
    m_globalScissorParams.scissorRegion.offset.x      = 0;
    m_globalScissorParams.scissorRegion.offset.y      = 0;
    m_globalScissorParams.scissorRegion.extent.width  = info.width;
    m_globalScissorParams.scissorRegion.extent.height = info.height;

    // If Imageless framebuffer don't process attachments
    if (info.flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT)
        return;

    for (uint32_t i = 0; i < m_attachmentCount; ++i)
    {
        const VkImageView& src        = info.pAttachments[i];
        Attachment* pAttachment       = &pAttachments[i];
        pAttachment->pView            = ImageView::ObjectFromHandle(info.pAttachments[i]);
        pAttachment->pImage           = pAttachment->pView->GetImage();
        pAttachment->viewFormat       = VkToPalFormat(pAttachment->pView->GetViewFormat(), m_settings);
        pAttachment->zRange           = pAttachment->pView->GetZRange();
        pAttachment->subresRangeCount = 0;

        const Image* pImage = pAttachment->pImage;

        SetSubresRanges(pImage, pAttachment);
    }
}

// =====================================================================================================================
// Destroy a framebuffer object
VkResult Framebuffer::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pDevice->FreeApiObject(pAllocator, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
// Set ImageViews for a Framebuffer attachment
void Framebuffer::SetImageViews(
    const VkImageView& imageView,
    Attachment*        pAttachments)
{
    pAttachments->pView  = ImageView::ObjectFromHandle(imageView);
    pAttachments->zRange = pAttachments->pView->GetZRange();

    const Image* pImage      = pAttachments->pView->GetImage();
    pAttachments->pImage     = pImage;
    pAttachments->viewFormat = VkToPalFormat(pImage->GetFormat(), m_settings);

    SetSubresRanges(pImage, pAttachments);
}

// =====================================================================================================================
// Set ImageViews for a Framebuffer attachment
void Framebuffer::SetImageViews(
    const VkRenderPassAttachmentBeginInfo* pRenderPassAttachmentBeginInfo)
{
    Attachment* pAttachments = static_cast<Attachment*>(Util::VoidPtrInc(this, GetAttachmentsOffset()));

    for (uint32_t i = 0; i < pRenderPassAttachmentBeginInfo->attachmentCount; i++)
    {
        SetImageViews(
            pRenderPassAttachmentBeginInfo->pAttachments[i],
            &(pAttachments[i]));
    }
}

// =====================================================================================================================
// Set ImageViews for a Framebuffer attachment
void Framebuffer::SetImageViews(
    const VkRenderingInfoKHR* pRenderingInfo)
{
    Attachment* pAttachments = static_cast<Attachment*>(Util::VoidPtrInc(this, GetAttachmentsOffset()));

    for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; i++)
    {
        SetImageViews(
            pRenderingInfo->pColorAttachments[i].imageView,
            &(pAttachments[i]));
     }
}

// =====================================================================================================================
// Update the subrange for framebuffer attachments
void Framebuffer::SetSubresRanges(
    const Image* pImage,
    Attachment*  pAttachment)
{
    // subresRangeCount and subresRange[] array in Framebuffer::attachment should be use to define the view range.
    // An imageview does not need to be a color target view or depth/stencil target view (it can be a shader read
    // only view, or something like that). In that case, we still need to define valid view range, based on the
    // format, for Vulkan-Pal interface to behavior correctly. AKA, subresRangeCount CANNOT be 0 since for that
    // PAL will not have proper range to work with when dealing with layout transition.

    // color format, one range
    if ((!(pImage->HasDepth())) &&
        (!(pImage->HasStencil())))
    {
        pAttachment->subresRangeCount = 1;
        pAttachment->pView->GetFrameBufferAttachmentSubresRange(&pAttachment->subresRange[0]);
    }
    else
    {
        // depth/stencil format, 1 or 2 range(s)
        uint32_t count = 0;

        if (pImage->HasDepth())
        {
            pAttachment->pView->GetFrameBufferAttachmentSubresRange(&pAttachment->subresRange[count]);
            pAttachment->subresRange[count].startSubres.plane = 0;
            count++;
        }

        if (pImage->HasStencil())
        {
            pAttachment->pView->GetFrameBufferAttachmentSubresRange(&pAttachment->subresRange[count]);
            pAttachment->subresRange[count].startSubres.plane = count;
            count++;
        }

        pAttachment->subresRangeCount = count;
    }

    VK_ASSERT(pAttachment->subresRangeCount > 0);
    VK_ASSERT(pAttachment->subresRange[0].numMips > 0);

    const Pal::ImageCreateInfo imageInfo = pAttachment->pImage->PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

    pAttachment->baseSubresExtent = ComputeLevelDimensions(
        imageInfo.extent,
        pAttachment->subresRange[0].startSubres.mipLevel);
}

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(
    VkDevice                                    device,
    VkFramebuffer                               framebuffer,
    const VkAllocationCallbacks*                pAllocator)
{
    if (framebuffer != VK_NULL_HANDLE)
    {
        Device*                      pDevice = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Framebuffer::ObjectFromHandle(framebuffer)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
