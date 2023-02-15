/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  barrier_filter_layer.cpp
* @brief Implementation of the Barrier Filter Layer.
***********************************************************************************************************************
*/

#include "barrier_filter_layer.h"

#include "include/vk_conv.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"

namespace vk
{

// =====================================================================================================================
BarrierFilterLayer::BarrierFilterLayer()
{
}

// =====================================================================================================================
BarrierFilterLayer::~BarrierFilterLayer()
{
}

namespace entry
{

namespace barrier_filter_layer
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    VkDependencyFlags                           dependencyFlags,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
    CmdBuffer*          pCmdBuffer    = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);
    BarrierFilterLayer* pLayer        = pCmdBuffer->VkDevice()->GetBarrierFilterLayer();
    const uint32_t      filterOptions = pCmdBuffer->VkDevice()->GetRuntimeSettings().barrierFilterOptions;

    {
        uint32_t memoryCount = memoryBarrierCount;
        uint32_t bufferCount = bufferMemoryBarrierCount;
        uint32_t imageCount  = imageMemoryBarrierCount;

        VkMemoryBarrier*       pMemory  = nullptr;
        VkBufferMemoryBarrier* pBuffers = nullptr;
        VkImageMemoryBarrier*  pImages  = nullptr;

        VirtualStackFrame virtStackFrame(pCmdBuffer->GetStackAllocator());

        if ((dstStageMask == VK_PIPELINE_STAGE_HOST_BIT) && ((filterOptions & FlushOnHostMask) != 0))
        {
            dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        if (memoryCount > 0)
        {
            pMemory = virtStackFrame.AllocArray<VkMemoryBarrier>(memoryCount);

            if (pMemory != nullptr)
            {
                // Reset the count and loop through to filter out memory barriers deemed unnecessary
                memoryCount = 0;

                for (uint32_t i = 0; i < memoryBarrierCount; ++i)
                {
                    if (((filterOptions & SkipDuplicateResourceBarriers) == 0) ||
                        (pMemoryBarriers[i].srcAccessMask != pMemoryBarriers[i].dstAccessMask))
                    {
                        pMemory[memoryCount++] = pMemoryBarriers[i];
                    }
                }
            }
        }

        if (bufferCount > 0)
        {
            pBuffers = virtStackFrame.AllocArray<VkBufferMemoryBarrier>(bufferCount);

            if (pBuffers != nullptr)
            {
                // Reset the count and loop through to filter out buffer memory barriers deemed unnecessary
                bufferCount = 0;

                for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i)
                {
                    if (((filterOptions & SkipDuplicateResourceBarriers) == 0) ||
                        (pBufferMemoryBarriers[i].srcAccessMask != pBufferMemoryBarriers[i].dstAccessMask) ||
                        (pBufferMemoryBarriers[i].srcQueueFamilyIndex != pBufferMemoryBarriers[i].dstQueueFamilyIndex))
                    {
                        pBuffers[bufferCount++] = pBufferMemoryBarriers[i];
                    }
                }
            }
        }

        if (imageCount > 0)
        {
            pImages = virtStackFrame.AllocArray<VkImageMemoryBarrier>(imageCount);

            if (pImages != nullptr)
            {
                // Reset the count and loop through to filter out image memory barriers deemed unnecessary
                imageCount = 0;

                for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i)
                {
                    if ((((filterOptions & SkipImageLayoutUndefined) == 0) ||
                         ((pImageMemoryBarriers[i].oldLayout != VK_IMAGE_LAYOUT_UNDEFINED) &&
                          (pImageMemoryBarriers[i].oldLayout != VK_IMAGE_LAYOUT_PREINITIALIZED)) ||
                         (pImageMemoryBarriers[i].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)) &&
                        (((filterOptions & SkipDuplicateResourceBarriers) == 0) ||
                         (pImageMemoryBarriers[i].oldLayout != pImageMemoryBarriers[i].newLayout) ||
                         (pImageMemoryBarriers[i].srcAccessMask != pImageMemoryBarriers[i].dstAccessMask) ||
                         (pImageMemoryBarriers[i].srcQueueFamilyIndex != pImageMemoryBarriers[i].dstQueueFamilyIndex)))
                    {
                        pImages[imageCount++] = pImageMemoryBarriers[i];
                    }
                }
            }
        }

        const uint32_t resourceBarrierCount = memoryCount + bufferCount + imageCount;

        if ((resourceBarrierCount > 0) || ((filterOptions & SkipStrayExecutionDependencies) == 0))
        {
            pLayer->GetNextLayer()->GetEntryPoints().vkCmdPipelineBarrier(
                cmdBuffer,
                srcStageMask,
                dstStageMask,
                dependencyFlags,
                memoryCount,
                (pMemory != nullptr) ? pMemory : pMemoryBarriers,
                bufferCount,
                (pBuffers != nullptr) ? pBuffers : pBufferMemoryBarriers,
                imageCount,
                (pImages != nullptr) ? pImages : pImageMemoryBarriers);
        }

        if (pMemory != nullptr)
        {
            virtStackFrame.FreeArray(pMemory);
        }

        if (pBuffers != nullptr)
        {
            virtStackFrame.FreeArray(pBuffers);
        }

        if (pImages != nullptr)
        {
            virtStackFrame.FreeArray(pImages);
        }
    }
}

} // namespace barrier_filter_layer

} // entry

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BARRIER_FILTER_LAYER_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::barrier_filter_layer::func_name

#define BARRIER_FILTER_LAYER_OVERRIDE_ENTRY(entry_name) \
    BARRIER_FILTER_LAYER_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void BarrierFilterLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    const RuntimeSettings& settings = pDispatchTable->GetDevice()->GetRuntimeSettings();

    // It is not useful to add this layer without any filter options set.
    VK_ASSERT(settings.barrierFilterOptions != BarrierFilterDisabled);

    if (settings.barrierFilterOptions & (SkipStrayExecutionDependencies |
                                         SkipImageLayoutUndefined       |
                                         SkipDuplicateResourceBarriers  |
                                         FlushOnHostMask))
    {
        BARRIER_FILTER_LAYER_OVERRIDE_ENTRY(vkCmdPipelineBarrier);
    }

}

} // namespace vk
