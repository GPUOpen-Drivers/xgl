/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
***********************************************************************************************************************
* @file  shadow_of_the_tomb_raider_layer.cpp
* @brief Implementation of the Shadow of the Tomb Raider Layer.
***********************************************************************************************************************
*/

#include "shadow_of_the_tomb_raider_layer.h"

#include "include/vk_device.h"

namespace vk
{

namespace entry
{

namespace shadow_of_the_tomb_raider_layer
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(
    VkDevice                      device,
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*  pAllocator,
    VkRenderPass*                 pRenderPass)
{
    Device*   pDevice = ApiDevice::ObjectFromHandle(device);
    OptLayer* pLayer  = pDevice->GetAppOptLayer();

    VkRenderPassCreateInfo createInfo = *pCreateInfo;
    VkSubpassDependency    subpassDeps[2];

    // This app issues a draw call during the following render pass and then a dispatch call that hangs on Strix1 and
    // StrixHalo.
    // The PS writes to a buffer that the CS then reads from without synchronization, causing the CS to loop infinitely.
    // The existing outgoing subpass dependency in this render pass uses srcStageMask = TOP_OF_PIPE and
    // dstStageMask = BOTTOM_OF_PIPE, which results in an empty barrier.
    // This swaps the outgoing subpass dependency's stage flags to ensure the PS writes are completed before CS reads.
    if ((pCreateInfo->flags                            == 0)                                                &&
        (pCreateInfo->attachmentCount                  == 1)                                                &&
        (pCreateInfo->subpassCount                     == 1)                                                &&
        (pCreateInfo->dependencyCount                  == 2)                                                &&
        (pCreateInfo->pAttachments[0].format           == VK_FORMAT_D16_UNORM)                              &&
        (pCreateInfo->pAttachments[0].samples          == 1)                                                &&
        (pCreateInfo->pAttachments[0].loadOp           == VK_ATTACHMENT_LOAD_OP_CLEAR)                      &&
        (pCreateInfo->pAttachments[0].storeOp          == VK_ATTACHMENT_STORE_OP_STORE)                     &&
        (pCreateInfo->pAttachments[0].stencilLoadOp    == VK_ATTACHMENT_LOAD_OP_LOAD)                       &&
        (pCreateInfo->pAttachments[0].stencilStoreOp   == VK_ATTACHMENT_STORE_OP_STORE)                     &&
        (pCreateInfo->pAttachments[0].initialLayout    == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) &&
        (pCreateInfo->pAttachments[0].finalLayout      == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) &&
        (pCreateInfo->pDependencies[0].srcSubpass      == VK_SUBPASS_EXTERNAL)                              &&
        (pCreateInfo->pDependencies[0].dstSubpass      == 0)                                                &&
        (pCreateInfo->pDependencies[0].srcStageMask    == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)                &&
        (pCreateInfo->pDependencies[0].dstStageMask    == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)             &&
        (pCreateInfo->pDependencies[0].srcAccessMask   == 0)                                                &&
        (pCreateInfo->pDependencies[0].dstAccessMask   == 0)                                                &&
        (pCreateInfo->pDependencies[0].dependencyFlags == 0)                                                &&
        (pCreateInfo->pDependencies[1].srcSubpass      == 0)                                                &&
        (pCreateInfo->pDependencies[1].dstSubpass      == VK_SUBPASS_EXTERNAL)                              &&
        (pCreateInfo->pDependencies[1].srcStageMask    == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)                &&
        (pCreateInfo->pDependencies[1].dstStageMask    == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)             &&
        (pCreateInfo->pDependencies[1].srcAccessMask   == 0)                                                &&
        (pCreateInfo->pDependencies[1].dstAccessMask   == 0)                                                &&
        (pCreateInfo->pDependencies[1].dependencyFlags == 0))
    {
        subpassDeps[0] = pCreateInfo->pDependencies[0];
        subpassDeps[1] = pCreateInfo->pDependencies[1];

        subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        createInfo.pDependencies = subpassDeps;
    }

    // Pass the call on to the Vulkan driver
    return pLayer->GetNextLayer()->GetEntryPoints().vkCreateRenderPass(
        device,
        &createInfo,
        pAllocator,
        pRenderPass);
}

} // namespace shadow_of_the_tomb_raider_layer

} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SHADOW_OF_THE_TOMB_RAIDER_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::shadow_of_the_tomb_raider_layer::func_name

#define SHADOW_OF_THE_TOMB_RAIDER_OVERRIDE_ENTRY(entry_name) SHADOW_OF_THE_TOMB_RAIDER_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void ShadowOfTheTombRaiderLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    SHADOW_OF_THE_TOMB_RAIDER_OVERRIDE_ENTRY(vkCreateRenderPass);
}

} // namespace vk
