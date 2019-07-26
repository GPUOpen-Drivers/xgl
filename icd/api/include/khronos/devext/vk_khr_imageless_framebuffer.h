/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 /**
  **********************************************************************************************************************
  * @file  vk_khr_imageless_framebuffer.h
  * @brief Temporary internal header for imageless framebuffer extension. Should be removed once the extension is
  *        published and the API gets included in the official Vulkan header.
  **********************************************************************************************************************
  */

#ifndef VK_KHR_IMAGELESS_FRAMEBUFFER_H_
#define VK_KHR_IMAGELESS_FRAMEBUFFER_H_

#ifndef VK_KHR_imageless_framebuffer
#include "vk_internal_ext_helper.h"

#define VK_KHR_imageless_framebuffer 1
#define VK_KHR_IMAGELESS_FRAMEBUFFER_SPEC_VERSION 1
#define VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME "VK_KHR_imageless_framebuffer"

#define VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NUMBER 109

#define VK_KHR_IMAGELESS_FRAMEBUFFER_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NUMBER, type, offset)

typedef struct VkPhysicalDeviceImagelessFramebufferFeaturesKHR {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           imagelessFramebuffer;
} VkPhysicalDeviceImagelessFramebufferFeaturesKHR;

typedef struct VkFramebufferAttachmentImageInfoKHR {
    VkStructureType       sType;
    const void*           pNext;
    VkImageCreateFlags    flags;
    VkImageUsageFlags     usage;
    uint32_t              width;
    uint32_t              height;
    uint32_t              layerCount;
    uint32_t              viewFormatCount;
    const VkFormat*       pViewFormats;
} VkFramebufferAttachmentImageInfoKHR;

typedef struct VkFramebufferAttachmentsCreateInfoKHR {
    VkStructureType                               sType;
    const void*                                   pNext;
    uint32_t                                      attachmentImageInfoCount;
    const VkFramebufferAttachmentImageInfoKHR*    pAttachmentImageInfos;
} VkFramebufferAttachmentsCreateInfoKHR;

typedef struct VkRenderPassAttachmentBeginInfoKHR {
    VkStructureType       sType;
    const void*           pNext;
    uint32_t              attachmentCount;
    const VkImageView*    pAttachments;
} VkRenderPassAttachmentBeginInfoKHR;

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR VK_KHR_IMAGELESS_FRAMEBUFFER_ENUM(VkStructureType, 0)
#define VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO_KHR            VK_KHR_IMAGELESS_FRAMEBUFFER_ENUM(VkStructureType, 1)
#define VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO_KHR              VK_KHR_IMAGELESS_FRAMEBUFFER_ENUM(VkStructureType, 2)
#define VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO_KHR              VK_KHR_IMAGELESS_FRAMEBUFFER_ENUM(VkStructureType, 3)

#define VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT_KHR 0x1

#endif // VK_KHR_imageless_framebuffer
#endif
