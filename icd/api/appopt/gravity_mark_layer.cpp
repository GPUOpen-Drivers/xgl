/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  gravity_mark_layer.cpp
* @brief Implementation Gravity Mark Layer.
***********************************************************************************************************************
*/

#include "gravity_mark_layer.h"

#include "include/vk_image.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"

namespace vk
{

namespace entry
{

namespace gravity_mark_layer
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
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);
    OptLayer*  pLayer     = pCmdBuffer->VkDevice()->GetAppOptLayer();

    // - corruption caused by incorrect barrier between CmdDispatch and CmdDrawIndexed calls which access the same
    // R16G16B16A16_SFLOAT image
    // - existing barrier from app specifies srcStageMask = TOP_OF_PIPE which is equivalent to VK_PIPELINE_STAGE_2_NONE
    // - changing this to BOTTOM_OF_PIPE will correctly sync between the dispatch and draw calls, resolving corruption

    if ((imageMemoryBarrierCount == 1)                                                                         &&
        (srcStageMask == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)                                                    &&
        (dstStageMask == (VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                       |  VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                       |  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                       |  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT))                                               &&
        (pImageMemoryBarriers != nullptr)                                                                      &&
        (Image::ObjectFromHandle(pImageMemoryBarriers[0].image)->GetFormat() == VK_FORMAT_R16G16B16A16_SFLOAT) &&
        (Image::ObjectFromHandle(pImageMemoryBarriers[0].image)->GetImageSamples() == VK_SAMPLE_COUNT_1_BIT)   &&
        (pImageMemoryBarriers[0].srcAccessMask == VK_ACCESS_NONE)                                              &&
        (pImageMemoryBarriers[0].dstAccessMask == VK_ACCESS_SHADER_READ_BIT)                                   &&
        (pImageMemoryBarriers[0].oldLayout == VK_IMAGE_LAYOUT_GENERAL)                                         &&
        (pImageMemoryBarriers[0].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
        srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    // Pass the barrier call on to the Vulkan driver
    pLayer->GetNextLayer()->GetEntryPoints().vkCmdPipelineBarrier(
        cmdBuffer,
        srcStageMask,
        dstStageMask,
        dependencyFlags,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

} // namespace gravity_mark_layer

} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define GRAVITY_MARK_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::gravity_mark_layer::func_name

#define GRAVITY_MARK_OVERRIDE_ENTRY(entry_name) GRAVITY_MARK_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void GravityMarkLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    GRAVITY_MARK_OVERRIDE_ENTRY(vkCmdPipelineBarrier);
}

} // namespace vk
