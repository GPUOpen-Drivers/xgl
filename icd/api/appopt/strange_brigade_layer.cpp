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
/**
***********************************************************************************************************************
* @file  strange_brigade_layer.cpp
* @brief Implementation Strange Brigade Layer.
***********************************************************************************************************************
*/

#include "strange_brigade_layer.h"

#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"

namespace vk
{

namespace entry
{

namespace strange_brigade_layer
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

    // We looked at cases where the image memory barrier count is less than 5
    // Which seemed to be the majority of barriers in this case.
    bool checkBarriers = (imageMemoryBarrierCount > 0 && imageMemoryBarrierCount < 5) ? true : false;

    if (checkBarriers)
    {
        bool pipelineBarrierEnabled = true;

        VkImageMemoryBarrier imageMemoryBarriers[4];

        for (uint32_t i = 0; i < imageMemoryBarrierCount; i++)
        {
            // Copy the image barrier
            imageMemoryBarriers[i] = pImageMemoryBarriers[i];

            // This app transitions from VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL to VK_IMAGE_LAYOUT_GENERAL
            // and than back again. Skipping these reduandant barriers in the app-layer for now.
            if ((imageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) &&
                (imageMemoryBarriers[i].newLayout == VK_IMAGE_LAYOUT_GENERAL))
            {
                pipelineBarrierEnabled = false;
            }
            else if ((imageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_GENERAL) &&
                     (imageMemoryBarriers[i].newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
            {
                pipelineBarrierEnabled = false;
            }
        }

        // Optionally skip the barrier here if it was detected to not have an impact
        if (pipelineBarrierEnabled)
        {
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
                imageMemoryBarriers);
        }
    }
    else
    {
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
}

} // namespace strange_brigade_layer

} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STRANGE_BRIGADE_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::strange_brigade_layer::func_name

#define STRANGE_BRIGADE_OVERRIDE_ENTRY(entry_name) STRANGE_BRIGADE_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void StrangeBrigadeLayer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    STRANGE_BRIGADE_OVERRIDE_ENTRY(vkCmdPipelineBarrier);
}

} // namespace vk
