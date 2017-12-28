/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace vk
{

using namespace Util;

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
    , m_attachments     (pAttachments)
{
    for (uint32_t i = 0; i < m_attachmentCount; ++i)
    {
        const VkImageView& src  = info.pAttachments[i];
        Attachment* pAttachment = &m_attachments[i];
        pAttachment->pView      = ImageView::ObjectFromHandle(info.pAttachments[i]);
        pAttachment->pImage     = pAttachment->pView->GetImage();
        pAttachment->viewFormat = VkToPalFormat(pAttachment->pView->GetViewFormat());
        pAttachment->zRange     = pAttachment->pView->GetZRange();

        pAttachment->subresRangeCount = 0;

        // subresRangeCount and subresRange[] array in Framebuffer::attachment should be use to define the view range.
        // An imageview does not need to be a color target view or depth/stencil target view (it can be a shader read
        // only view, or something like that). In that case, we still need to define valid view range, based on the
        // format, for Vulkan-Pal interface to behavior correctly. AKA, subresRangeCount CANNOT be 0 since for that
        // PAL will not have proper range to work with when dealing with layout transition.

        const VkFormat imgFormat = pAttachment->pImage->GetFormat();

        // color format, one range
        if ((!(Formats::HasDepth(imgFormat))) &&
            (!(Formats::HasStencil(imgFormat))))
        {
            pAttachment->subresRangeCount = 1;
            pAttachment->pView->GetFrameBufferAttachmentSubresRange(Pal::ImageAspect::Color,
                                                                    &pAttachment->subresRange[0]);
        }
        else
        {
            // depth/stencil format, 1 or 2 range(s)
            uint32_t count = 0;

            if (Formats::HasDepth(imgFormat))
            {
                pAttachment->pView->GetFrameBufferAttachmentSubresRange(Pal::ImageAspect::Depth,
                                                                        &pAttachment->subresRange[count]);
                count++;
            }

            if (Formats::HasStencil(imgFormat))
            {
                pAttachment->pView->GetFrameBufferAttachmentSubresRange(Pal::ImageAspect::Stencil,
                                                                        &pAttachment->subresRange[count]);
                count++;
            }

            pAttachment->subresRangeCount = count;
        }

        VK_ASSERT(pAttachment->subresRangeCount > 0);
        VK_ASSERT(pAttachment->subresRange[0].numMips > 0);

        const Pal::ImageCreateInfo imageInfo = pAttachment->pImage->PalImage()->GetImageCreateInfo();

        pAttachment->baseSubresExtent = ComputeLevelDimensions(
            imageInfo.extent,
            pAttachment->subresRange[0].startSubres.mipLevel);
    }
}

// =====================================================================================================================
// Does the given clear box cover the entire subresource range of the attachment or only partially covered?
bool Framebuffer::IsPartialClear(
    const Pal::Box&                box,
    const Framebuffer::Attachment& attachment)
{
    VK_ASSERT(attachment.subresRangeCount == 1 ||
              attachment.subresRange[0].numSlices == attachment.subresRange[1].numSlices);

    bool isPartialClear = ((box.offset.x != 0) ||
                           (box.offset.y != 0) ||
                           (box.extent.width  != attachment.baseSubresExtent.width) ||
                           (box.extent.height != attachment.baseSubresExtent.height));

    if (attachment.pImage->Is2dArrayCompatible())
    {
        // VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR is used to create a 2D or 2D array view of a 3D texture, so this
        // case is also a partial clear given all of the depth slices are part of the same subresource in PAL.
        if ((box.offset.z != 0) ||
            (box.extent.depth != attachment.baseSubresExtent.depth))
        {
            isPartialClear = true;
        }
    }
    // For other images, each layer is considered a separate subresource.

    return isPartialClear;
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
