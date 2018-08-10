/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  VK_KHR_create_renderpass2.h
 * @brief Temporary internal header for KHR_create_renderpass2 extension. Should be removed once the extension is published
 *        and the API gets included in the official Vulkan header.
 **********************************************************************************************************************
 */
#ifndef VK_KHR_CREATE_RENDERPASS2_H_
#define VK_KHR_CREATE_RENDERPASS2_H_

#include "vk_internal_ext_helper.h"

#define VK_KHR_create_renderpass2                   1
#define VK_KHR_CREATE_RENDERPASS2_SPEC_VERSION      1
#define VK_KHR_CREATE_RENDERPASS2_EXTENSION_NAME    "VK_KHR_create_renderpass2"

#define VK_KHR_CREATE_RENDERPASS2_EXTENSION_NUMBER  110

#define VK_KHR_CREATE_RENDERPASS2_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_KHR_CREATE_RENDERPASS2_EXTENSION_NUMBER, type, offset)

#define VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR  VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 0)
#define VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR    VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 1)
#define VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR     VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 2)
#define VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR      VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 3)
#define VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 4)
#define VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR        VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 5)
#define VK_STRUCTURE_TYPE_SUBPASS_END_INFO_KHR          VK_KHR_CREATE_RENDERPASS2_ENUM(VkStructureType, 6)

typedef struct VkAttachmentDescription2KHR {
    VkStructureType                 sType;
    const void*                     pNext;
    VkAttachmentDescriptionFlags    flags;
    VkFormat                        format;
    VkSampleCountFlagBits           samples;
    VkAttachmentLoadOp              loadOp;
    VkAttachmentStoreOp             storeOp;
    VkAttachmentLoadOp              stencilLoadOp;
    VkAttachmentStoreOp             stencilStoreOp;
    VkImageLayout                   initialLayout;
    VkImageLayout                   finalLayout;
} VkAttachmentDescription2KHR;

typedef struct VkAttachmentReference2KHR {
    VkStructureType       sType;
    const void*           pNext;
    uint32_t              attachment;
    VkImageLayout         layout;
    VkImageAspectFlags    aspectMask;
} VkAttachmentReference2KHR;

typedef struct VkSubpassDescription2KHR {
    VkStructureType                     sType;
    const void*                         pNext;
    VkSubpassDescriptionFlags           flags;
    VkPipelineBindPoint                 pipelineBindPoint;
    uint32_t                            viewMask;
    uint32_t                            inputAttachmentCount;
    const VkAttachmentReference2KHR*    pInputAttachments;
    uint32_t                            colorAttachmentCount;
    const VkAttachmentReference2KHR*    pColorAttachments;
    const VkAttachmentReference2KHR*    pResolveAttachments;
    const VkAttachmentReference2KHR*    pDepthStencilAttachment;
    uint32_t                            preserveAttachmentCount;
    const uint32_t*                     pPreserveAttachments;
} VkSubpassDescription2KHR;

typedef struct VkSubpassDependency2KHR {
    VkStructureType         sType;
    const void*             pNext;
    uint32_t                srcSubpass;
    uint32_t                dstSubpass;
    VkPipelineStageFlags    srcStageMask;
    VkPipelineStageFlags    dstStageMask;
    VkAccessFlags           srcAccessMask;
    VkAccessFlags           dstAccessMask;
    VkDependencyFlags       dependencyFlags;
    int32_t                 viewOffset;
} VkSubpassDependency2KHR;

typedef struct VkRenderPassCreateInfo2KHR {
    VkStructureType                       sType;
    const void*                           pNext;
    VkRenderPassCreateFlags               flags;
    uint32_t                              attachmentCount;
    const VkAttachmentDescription2KHR*    pAttachments;
    uint32_t                              subpassCount;
    const VkSubpassDescription2KHR*       pSubpasses;
    uint32_t                              dependencyCount;
    const VkSubpassDependency2KHR*        pDependencies;
    uint32_t                              correlatedViewMaskCount;
    const uint32_t*                       pCorrelatedViewMasks;
} VkRenderPassCreateInfo2KHR;

typedef struct VkSubpassBeginInfoKHR {
    VkStructureType      sType;
    const void*          pNext;
    VkSubpassContents    contents;
} VkSubpassBeginInfoKHR;

typedef struct VkSubpassEndInfoKHR {
    VkStructureType    sType;
    const void*        pNext;
} VkSubpassEndInfoKHR;

typedef VkResult (VKAPI_PTR *PFN_vkCreateRenderPass2KHR)(VkDevice device, const VkRenderPassCreateInfo2KHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass);
typedef void (VKAPI_PTR *PFN_vkCmdBeginRenderPass2KHR)(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo*      pRenderPassBegin, const VkSubpassBeginInfoKHR*      pSubpassBeginInfo);
typedef void (VKAPI_PTR *PFN_vkCmdNextSubpass2KHR)(VkCommandBuffer commandBuffer, const VkSubpassBeginInfoKHR*      pSubpassBeginInfo, const VkSubpassEndInfoKHR*        pSubpassEndInfo);
typedef void (VKAPI_PTR *PFN_vkCmdEndRenderPass2KHR)(VkCommandBuffer commandBuffer, const VkSubpassEndInfoKHR*        pSubpassEndInfo);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass2KHR(
    VkDevice                                    device,
    const VkRenderPassCreateInfo2KHR*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkRenderPass*                               pRenderPass);

VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2KHR(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    const VkSubpassBeginInfoKHR*                pSubpassBeginInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2KHR(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassBeginInfoKHR*                pSubpassBeginInfo,
    const VkSubpassEndInfoKHR*                  pSubpassEndInfo);

VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2KHR(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassEndInfoKHR*                  pSubpassEndInfo);
#endif

#endif /* VK_KHR_CREATE_RENDERPASS2_H_ */
