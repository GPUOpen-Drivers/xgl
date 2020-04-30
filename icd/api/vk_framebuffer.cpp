/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
        const Pal::ImageAspect attachmentAspect = subresRange[i].startSubres.aspect;

        const bool   colorAvailable = (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) &&
                                      (attachmentAspect == Pal::ImageAspect::Color);

        const bool   depthAvailable = (aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) &&
                                      (attachmentAspect == Pal::ImageAspect::Depth);

        const bool stencilAvailable = (aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                                      (attachmentAspect == Pal::ImageAspect::Stencil);

        const bool     yuvAvailable = (aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) &&
                                      ((attachmentAspect == Pal::ImageAspect::Y)    ||
                                       (attachmentAspect == Pal::ImageAspect::CbCr) ||
                                       (attachmentAspect == Pal::ImageAspect::Cb)   ||
                                       (attachmentAspect == Pal::ImageAspect::Cr)   ||
                                       (attachmentAspect == Pal::ImageAspect::YCbCr));

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

    const size_t apiSize        = sizeof(Framebuffer);
    const size_t attachmentSize = sizeof(Attachment) * pCreateInfo->attachmentCount;
    const size_t objSize        = apiSize + attachmentSize;

    void* pSystemMem = pDevice->AllocApiObject(objSize, pAllocator);

    // On success, wrap it up in a Vulkan object and return.
    if (pSystemMem != nullptr)
    {
        Attachment* pAttachments = static_cast<Attachment*>(Util::VoidPtrInc(pSystemMem, apiSize));

        VK_PLACEMENT_NEW(pSystemMem) Framebuffer(*pCreateInfo, pAttachments);

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
                         Attachment*                    pAttachments)
    : m_attachmentCount (info.attachmentCount)
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
        pAttachment->viewFormat       = VkToPalFormat(pAttachment->pView->GetViewFormat());
        pAttachment->zRange           = pAttachment->pView->GetZRange();
        pAttachment->subresRangeCount = 0;

        const Image* pImage = pAttachment->pImage;

        SetSubresRanges(pImage, pAttachment);
    }
}

// =====================================================================================================================
// Destroy a framebuffer object
VkResult Framebuffer::Destroy(
    const Device*                   pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    // Call destructor
    Util::Destructor(this);

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
// Set ImageViews for a Framebuffer attachment
void Framebuffer::SetImageViews(
    const VkRenderPassAttachmentBeginInfo* pRenderPassAttachmentBeginInfo)
{
    Attachment* pAttachments = static_cast<Attachment*>(Util::VoidPtrInc(this, sizeof(*this)));

    for (uint32_t i = 0; i < pRenderPassAttachmentBeginInfo->attachmentCount; i++)
    {
        pAttachments[i].pView  = ImageView::ObjectFromHandle(pRenderPassAttachmentBeginInfo->pAttachments[i]);
        pAttachments[i].zRange = pAttachments[i].pView->GetZRange();

        const Image* pImage        = pAttachments[i].pView->GetImage();
        pAttachments[i].pImage     = pImage;
        pAttachments[i].viewFormat = VkToPalFormat(pImage->GetFormat());

        SetSubresRanges(pImage, &pAttachments[i]);
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
            pAttachment->subresRange[count].startSubres.aspect = Pal::ImageAspect::Depth;
            count++;
        }

        if (pImage->HasStencil())
        {
            pAttachment->pView->GetFrameBufferAttachmentSubresRange(&pAttachment->subresRange[count]);
            pAttachment->subresRange[count].startSubres.aspect = Pal::ImageAspect::Stencil;
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
        const Device*                pDevice = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        Framebuffer::ObjectFromHandle(framebuffer)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
