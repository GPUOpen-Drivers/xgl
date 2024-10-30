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
* @file  baldurs_gate3_layer.cpp
* @brief Implementation Baldur's Gate 3 Layer.
***********************************************************************************************************************
*/

#include "baldurs_gate3_layer.h"

#include "include/vk_image.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"

namespace vk
{

namespace entry
{

namespace baldurs_gate3_layer
{

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2KHR(
    VkCommandBuffer                             cmdBuffer,
    const VkDependencyInfoKHR*                  pDependencyInfo)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(cmdBuffer);
    OptLayer*  pLayer     = pCmdBuffer->VkDevice()->GetAppOptLayer();

    bool needsBarrierOverride = false;

    VkDependencyInfoKHR      dependencyInfo = *pDependencyInfo;
    VkImageMemoryBarrier2KHR imageBarriers[3];

    const VkDependencyInfoKHR* pOverrideDependencyInfo   = &dependencyInfo;
    const VkImageMemoryBarrier2KHR* pImageMemoryBarriers = &imageBarriers[0];

    if ((pDependencyInfo->memoryBarrierCount       == 0)       &&
        (pDependencyInfo->bufferMemoryBarrierCount == 0)       &&
        (pDependencyInfo->imageMemoryBarrierCount  == 3)       &&
        (pDependencyInfo->pImageMemoryBarriers     != nullptr) &&
        (pDependencyInfo->pImageMemoryBarriers[2].srcStageMask  == VK_PIPELINE_STAGE_2_COPY_BIT_KHR)     &&
        (pDependencyInfo->pImageMemoryBarriers[2].dstStageMask  == VK_PIPELINE_STAGE_2_COPY_BIT_KHR)     &&
        (pDependencyInfo->pImageMemoryBarriers[2].srcAccessMask == VK_ACCESS_2_TRANSFER_READ_BIT_KHR)    &&
        (pDependencyInfo->pImageMemoryBarriers[2].dstAccessMask == VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR)   &&
        (pDependencyInfo->pImageMemoryBarriers[2].oldLayout     == VK_IMAGE_LAYOUT_UNDEFINED)            &&
        (pDependencyInfo->pImageMemoryBarriers[2].newLayout     == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) &&
        (Image::ObjectFromHandle
            (pDependencyInfo->pImageMemoryBarriers[2].image)->GetFormat() == VK_FORMAT_B10G11R11_UFLOAT_PACK32) &&
        (Image::ObjectFromHandle
            (pDependencyInfo->pImageMemoryBarriers[2].image)->GetImageSamples() == VK_SAMPLE_COUNT_1_BIT))
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            imageBarriers[i] = dependencyInfo.pImageMemoryBarriers[i];
        }

        imageBarriers[2].srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR;

        dependencyInfo.pImageMemoryBarriers = pImageMemoryBarriers;

        needsBarrierOverride = true;
    }

    // Pass the barrier call on to the Vulkan driver
    pLayer->GetNextLayer()->GetEntryPoints().vkCmdPipelineBarrier2KHR(
        cmdBuffer,
        (needsBarrierOverride) ? pOverrideDependencyInfo : pDependencyInfo);
}

} // namespace baldurs_gate3_layer

} // namespace entry

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define BALDURS_GATE3_OVERRIDE_ALIAS(entry_name, func_name) \
    pDispatchTable->OverrideEntryPoints()->entry_name = vk::entry::baldurs_gate3_layer::func_name

#define BALDURS_GATE3_OVERRIDE_ENTRY(entry_name) BALDURS_GATE3_OVERRIDE_ALIAS(entry_name, entry_name)

// =====================================================================================================================
void BaldursGate3Layer::OverrideDispatchTable(
    DispatchTable* pDispatchTable)
{
    // Save current device dispatch table to use as the next layer.
    m_nextLayer = *pDispatchTable;

    BALDURS_GATE3_OVERRIDE_ENTRY(vkCmdPipelineBarrier2KHR);
}

} // namespace vk
