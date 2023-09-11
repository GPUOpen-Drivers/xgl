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
************************************************************************************************************************
* @file  entry.cpp
* @brief Command buffer entry functions
************************************************************************************************************************
*/

#include "include/vk_cmdbuffer.h"
#include "include/vk_query.h"

namespace vk
{

namespace entry
{

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(
    VkCommandBuffer                             cmdBuffer,
    const VkCommandBufferBeginInfo*             pBeginInfo)
{
    return ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->Begin(pBeginInfo);
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(
    VkCommandBuffer                             cmdBuffer)
{
    return ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->End();
}

// =====================================================================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkCommandBufferResetFlags                   flags)
{
    return ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->Reset(flags);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipeline                                  pipeline)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BindPipeline(pipelineBindPoint, pipeline);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->VkDevice()->GetEntryPoints().vkCmdBindDescriptorSets(
        cmdBuffer,
        pipelineBindPoint,
        layout,
        firstSet,
        descriptorSetCount,
        pDescriptorSets,
        dynamicOffsetCount,
        pDynamicOffsets);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkIndexType                                 indexType)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BindIndexBuffer(
        buffer,
        offset,
        VK_WHOLE_SIZE,
        indexType);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BindVertexBuffers(
        firstBinding,
        bindingCount,
        pBuffers,
        pOffsets,
        nullptr,
        nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDraw(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    vertexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstVertex,
    uint32_t                                    firstInstance)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->Draw(
        firstVertex,
        vertexCount,
        firstInstance,
        instanceCount);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    indexCount,
    uint32_t                                    instanceCount,
    uint32_t                                    firstIndex,
    int32_t                                     vertexOffset,
    uint32_t                                    firstInstance)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DrawIndexed(
        firstIndex,
        indexCount,
        vertexOffset,
        firstInstance,
        instanceCount);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    constexpr bool Indexed       = false;
    constexpr bool BufferedCount = false;

    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DrawIndirect<Indexed, BufferedCount>(
        buffer,
        offset,
        drawCount,
        stride,
        VK_NULL_HANDLE,
        0);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    constexpr bool Indexed       = true;
    constexpr bool BufferedCount = false;

    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DrawIndirect<Indexed, BufferedCount>(
        buffer,
        offset,
        drawCount,
        stride,
        VK_NULL_HANDLE,
        0);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCount(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    constexpr bool Indexed       = false;
    constexpr bool BufferedCount = true;

    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DrawIndirect<Indexed, BufferedCount>(
        buffer,
        offset,
        maxDrawCount,
        stride,
        countBuffer,
        countOffset);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCount(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    constexpr bool Indexed       = true;
    constexpr bool BufferedCount = true;

    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DrawIndirect<Indexed, BufferedCount>(
        buffer,
        offset,
        maxDrawCount,
        stride,
        countBuffer,
        countOffset);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->DrawMeshTasks(
        groupCountX,
        groupCountY,
        groupCountZ);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectEXT(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    uint32_t                                    drawCount,
    uint32_t                                    stride)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->DrawMeshTasksIndirect<false>(
        buffer,
        offset,
        drawCount,
        stride,
        VK_NULL_HANDLE,
        0);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDrawMeshTasksIndirectCountEXT(
    VkCommandBuffer                             commandBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset,
    VkBuffer                                    countBuffer,
    VkDeviceSize                                countBufferOffset,
    uint32_t                                    maxDrawCount,
    uint32_t                                    stride)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->DrawMeshTasksIndirect<true>(
        buffer,
        offset,
        maxDrawCount,
        stride,
        countBuffer,
        countBufferOffset);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    x,
    uint32_t                                    y,
    uint32_t                                    z)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->Dispatch(x, y, z);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    buffer,
    VkDeviceSize                                offset)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->DispatchIndirect(buffer, offset);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferCopy*                         pRegions)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->CopyBuffer(
        srcBuffer,
        dstBuffer,
        regionCount,
        pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageCopy*                          pRegions)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->CopyImage(
        srcImage,
        srcImageLayout,
        dstImage,
        dstImageLayout,
        regionCount,
        pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageBlit*                          pRegions,
    VkFilter                                    filter)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BlitImage(
        srcImage,
        srcImageLayout,
        dstImage,
        dstImageLayout,
        regionCount,
        pRegions,
        filter);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    srcBuffer,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->CopyBufferToImage(
        srcBuffer,
        dstImage,
        dstImageLayout,
        regionCount,
        pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkBuffer                                    dstBuffer,
    uint32_t                                    regionCount,
    const VkBufferImageCopy*                    pRegions)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->CopyImageToBuffer(
        srcImage,
        srcImageLayout,
        dstBuffer,
        regionCount,
        pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdUpdateBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                dataSize,
    const void*                                 pData)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->UpdateBuffer(
        dstBuffer,
        dstOffset,
        dataSize,
        static_cast<const uint32_t*>(pData));
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(
    VkCommandBuffer                             cmdBuffer,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                size,
    uint32_t                                    data)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->FillBuffer(
        dstBuffer,
        dstOffset,
        size,
        data);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearColorImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearColorValue*                    pColor,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ClearColorImage(
        image,
        imageLayout,
        pColor,
        rangeCount,
        pRanges);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearDepthStencilImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     image,
    VkImageLayout                               imageLayout,
    const VkClearDepthStencilValue*             pDepthStencil,
    uint32_t                                    rangeCount,
    const VkImageSubresourceRange*              pRanges)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ClearDepthStencilImage(
        image,
        imageLayout,
        pDepthStencil->depth,
        pDepthStencil->stencil,
        rangeCount,
        pRanges);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    attachmentCount,
    const VkClearAttachment*                    pAttachments,
    uint32_t                                    rectCount,
    const VkClearRect*                          pRects)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ClearAttachments(
        attachmentCount,
        pAttachments,
        rectCount,
        pRects);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage(
    VkCommandBuffer                             cmdBuffer,
    VkImage                                     srcImage,
    VkImageLayout                               srcImageLayout,
    VkImage                                     dstImage,
    VkImageLayout                               dstImageLayout,
    uint32_t                                    regionCount,
    const VkImageResolve*                       pRegions)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ResolveImage(srcImage,
                                                            srcImageLayout,
                                                            dstImage,
                                                            dstImageLayout,
                                                            regionCount,
                                                            pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent(
    VkCommandBuffer                             cmdBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetEvent(event, stageMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent(
    VkCommandBuffer                             cmdBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags                        stageMask)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ResetEvent(event, stageMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    VkPipelineStageFlags                        srcStageMask,
    VkPipelineStageFlags                        dstStageMask,
    uint32_t                                    memoryBarrierCount,
    const VkMemoryBarrier*                      pMemoryBarriers,
    uint32_t                                    bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
    uint32_t                                    imageMemoryBarrierCount,
    const VkImageMemoryBarrier*                 pImageMemoryBarriers)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->WaitEvents(
        eventCount,
        pEvents,
        srcStageMask,
        dstStageMask,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

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
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->PipelineBarrier(
        srcStageMask,
        dstStageMask,
        memoryBarrierCount,
        pMemoryBarriers,
        bufferMemoryBarrierCount,
        pBufferMemoryBarriers,
        imageMemoryBarrierCount,
        pImageMemoryBarriers);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginQuery(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    VkQueryControlFlags                         flags)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BeginQueryIndexed(queryPool, query, flags, 0);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->EndQueryIndexed(queryPool, query, 0);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ResetQueryPool(queryPool,
                                                              firstQuery,
                                                              queryCount);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineStageFlagBits                     pipelineStage,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->WriteTimestamp(
        pipelineStage,
        QueryPool::ObjectFromHandle(queryPool)->AsTimestampQueryPool(),
        query);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyQueryPoolResults(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery,
    uint32_t                                    queryCount,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    VkDeviceSize                                stride,
    VkQueryResultFlags                          flags)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->CopyQueryPoolResults(queryPool,
                                                                    firstQuery,
                                                                    queryCount,
                                                                    dstBuffer,
                                                                    dstOffset,
                                                                    stride,
                                                                    flags);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    offset,
    uint32_t                                    size,
    const void*                                 pValues)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->PushConstants(layout,
        stageFlags,
        offset,
        size,
        pValues);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BeginRenderPass(pRenderPassBegin, contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass2(
    VkCommandBuffer                             commandBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BeginRenderPass(pRenderPassBegin, pSubpassBeginInfo->contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer                             commandBuffer,
    VkSubpassContents                           contents)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->NextSubPass(contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass2(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassBeginInfo*                   pSubpassBeginInfo,
    const VkSubpassEndInfo*                     pSubpassEndInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->NextSubPass(pSubpassBeginInfo->contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             commandBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->EndRenderPass();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass2(
    VkCommandBuffer                             commandBuffer,
    const VkSubpassEndInfo*                     pSubpassEndInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->EndRenderPass();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->ExecuteCommands(commandBufferCount, pCommandBuffers);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(
    VkDevice                                    device,
    VkCommandPool                               commandPool,
    uint32_t                                    commandBufferCount,
    const VkCommandBuffer*                      pCommandBuffers)
{
    for(uint32_t i = 0;i < commandBufferCount; ++i)
    {
        if (pCommandBuffers[i] != VK_NULL_HANDLE)
        {
            ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[i])->Destroy();
        }
    }
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBase(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    baseGroupX,
    uint32_t                                    baseGroupY,
    uint32_t                                    baseGroupZ,
    uint32_t                                    groupCountX,
    uint32_t                                    groupCountY,
    uint32_t                                    groupCountZ)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->DispatchOffset(baseGroupX, baseGroupY, baseGroupZ,
                                                                  groupCountX, groupCountY, groupCountZ);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMask(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDeviceMask(deviceMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstViewport,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetViewport(firstViewport, viewportCount, pViewports);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(
    VkCommandBuffer                             cmdBuffer,
    uint32_t                                    firstScissor,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetScissor(firstScissor, scissorCount, pScissors);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineWidth(
    VkCommandBuffer                             cmdBuffer,
    float                                       lineWidth)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetLineWidth(lineWidth);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBias(
    VkCommandBuffer                             cmdBuffer,
    float                                       depthBiasConstantFactor,
    float                                       depthBiasClamp,
    float                                       depthBiasSlopeFactor)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetDepthBias(depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetBlendConstants(
    VkCommandBuffer                             cmdBuffer,
    const float                                 blendConstants[4])
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetBlendConstants(blendConstants);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBounds(
    VkCommandBuffer                             cmdBuffer,
    float                                       minDepthBounds,
    float                                       maxDepthBounds)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetDepthBounds(minDepthBounds, maxDepthBounds);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilCompareMask(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    compareMask)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetStencilCompareMask(faceMask, compareMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilWriteMask(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    writeMask)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetStencilWriteMask(faceMask, writeMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilReference(
    VkCommandBuffer                             cmdBuffer,
    VkStencilFaceFlags                          faceMask,
    uint32_t                                    reference)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->SetStencilReference(faceMask, reference);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdDebugMarkerBegin(pMarkerInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT(
    VkCommandBuffer                             commandBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdDebugMarkerEnd();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
{
    // The SQTT layer shadows this extension's functions and contains extra code to make use of them.  This
    // extension is not enabled when the SQTT layer is not also enabled, so these functions are currently
    // just blank placeholder functions in case there will be a time where we need to do something with them
    // on this path also.
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pLabelInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdBeginDebugUtilsLabel(pLabelInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdEndDebugUtilsLabel();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
    VkCommandBuffer                             commandBuffer,
    const VkDebugUtilsLabelEXT*                 pLabelInfo)
{
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleLocationsEXT(
    VkCommandBuffer                         commandBuffer,
    const VkSampleLocationsInfoEXT*         pSampleLocationsInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetSampleLocations(pSampleLocationsInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarkerAMD(
    VkCommandBuffer         commandBuffer,
    VkPipelineStageFlagBits pipelineStage,
    VkBuffer                dstBuffer,
    VkDeviceSize            dstOffset,
    uint32_t                marker)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->WriteBufferMarker(pipelineStage, dstBuffer, dstOffset, marker);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindTransformFeedbackBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BindTransformFeedbackBuffers(firstBinding,
                                                                                bindingCount,
                                                                                pBuffers,
                                                                                pOffsets,
                                                                                pSizes);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BeginTransformFeedback(firstCounterBuffer,
                                                                          counterBufferCount,
                                                                          pCounterBuffers,
                                                                          pCounterBufferOffsets);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndTransformFeedbackEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstCounterBuffer,
    uint32_t                                    counterBufferCount,
    const VkBuffer*                             pCounterBuffers,
    const VkDeviceSize*                         pCounterBufferOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->EndTransformFeedback(firstCounterBuffer,
                                                                        counterBufferCount,
                                                                        pCounterBuffers,
                                                                        pCounterBufferOffsets);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginQueryIndexedEXT(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    VkQueryControlFlags                         flags,
    uint32_t                                    index)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BeginQueryIndexed(queryPool, query, flags, index);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndQueryIndexedEXT(
    VkCommandBuffer                             commandBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query,
    uint32_t                                    index)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->EndQueryIndexed(queryPool, query, index);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectByteCountEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    instanceCount,
    uint32_t                                    firstInstance,
    VkBuffer                                    counterBuffer,
    VkDeviceSize                                counterBufferOffset,
    uint32_t                                    counterOffset,
    uint32_t                                    vertexStride)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->DrawIndirectByteCount(instanceCount,
                                                                         firstInstance,
                                                                         counterBuffer,
                                                                         counterBufferOffset,
                                                                         counterOffset,
                                                                         vertexStride);
}

#if VKI_RAY_TRACING
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer                                         commandBuffer,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BuildAccelerationStructures(
        infoCount,
        pInfos,
        ppBuildRangeInfos,
        nullptr,
        nullptr,
        nullptr);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer                                    commandBuffer,
    uint32                                             infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkDeviceAddress*                             pIndirectDeviceAddresses,
    const uint32*                                      pIndirectStrides,
    const uint32* const*                               ppMaxPrimitiveCounts)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BuildAccelerationStructures(
        infoCount,
        pInfos,
        nullptr,
        pIndirectDeviceAddresses,
        pIndirectStrides,
        ppMaxPrimitiveCounts);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    uint32_t                                    width,
    uint32_t                                    height,
    uint32_t                                    depth)
{
     ApiCmdBuffer::ObjectFromHandle(commandBuffer)->TraceRays(
         *pRaygenShaderBindingTable,
         *pMissShaderBindingTable,
         *pHitShaderBindingTable,
         *pCallableShaderBindingTable,
         width,
         height,
         depth);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirectKHR(
    VkCommandBuffer                             commandBuffer,
    const VkStridedDeviceAddressRegionKHR*      pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR*      pCallableShaderBindingTable,
    VkDeviceAddress                             indirectDeviceAddress)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->TraceRaysIndirect(
         GpuRt::ExecuteIndirectArgType::DispatchDimensions,
         *pRaygenShaderBindingTable,
         *pMissShaderBindingTable,
         *pHitShaderBindingTable,
         *pCallableShaderBindingTable,
         indirectDeviceAddress);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureKHR(
    VkCommandBuffer                             commandBuffer,
    const VkCopyAccelerationStructureInfoKHR*   pInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyAccelerationStructure(pInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteAccelerationStructuresPropertiesKHR(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    accelerationStructureCount,
    const VkAccelerationStructureKHR*           pAccelerationStructures,
    VkQueryType                                 queryType,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->WriteAccelerationStructuresProperties(
        accelerationStructureCount,
        pAccelerationStructures,
        queryType,
        queryPool,
        firstQuery);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer                                   commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyAccelerationStructureToMemory(pInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer                                   commandBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyMemoryToAccelerationStructure(pInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetRayTracingPipelineStackSizeKHR(
    VkCommandBuffer                                   commandBuffer,
    uint32_t                                          pipelineStackSize)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetRayTracingPipelineStackSize(pipelineStackSize);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirect2KHR(
    VkCommandBuffer                             commandBuffer,
    VkDeviceAddress                             indirectDeviceAddress)
{
    VkStridedDeviceAddressRegionKHR emptyShaderBindingTable = {};

    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->TraceRaysIndirect(
        GpuRt::ExecuteIndirectArgType::DispatchDimenionsAndShaderTable,
        emptyShaderBindingTable,
        emptyShaderBindingTable,
        emptyShaderBindingTable,
        emptyShaderBindingTable,
        indirectDeviceAddress);
}
#endif

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    lineStippleFactor,
    uint16_t                                    lineStipplePattern)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetLineStippleEXT(
        lineStippleFactor,
        lineStipplePattern);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetFragmentShadingRateKHR(
    VkCommandBuffer                          commandBuffer,
    const VkExtent2D*                        pFragmentSize,
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdSetPerDrawVrsRate(pFragmentSize, combinerOps);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginConditionalRenderingEXT(
    VkCommandBuffer                           commandBuffer,
    const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdBeginConditionalRendering(pConditionalRenderingBegin);
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndConditionalRenderingEXT(
    VkCommandBuffer                           commandBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CmdEndConditionalRendering();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent2(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    const VkDependencyInfoKHR*                  pDependencyInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetEvent2(event, pDependencyInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent2(
    VkCommandBuffer                             commandBuffer,
    VkEvent                                     event,
    VkPipelineStageFlags2KHR                    stageMask)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->ResetEvent(event, stageMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWaitEvents2(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    eventCount,
    const VkEvent*                              pEvents,
    const VkDependencyInfoKHR*                  pDependencyInfos)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->WaitEvents2(eventCount, pEvents, pDependencyInfos);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(
    VkCommandBuffer                             commandBuffer,
    const VkDependencyInfoKHR*                  pDependencyInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->PipelineBarrier2(pDependencyInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp2(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags2KHR                    stage,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->WriteTimestamp(
        stage,
        QueryPool::ObjectFromHandle(queryPool)->AsTimestampQueryPool(),
        query);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdWriteBufferMarker2AMD(
    VkCommandBuffer                             commandBuffer,
    VkPipelineStageFlags2KHR                    stage,
    VkBuffer                                    dstBuffer,
    VkDeviceSize                                dstOffset,
    uint32_t                                    marker)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->WriteBufferMarker(stage, dstBuffer, dstOffset, marker);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginRendering(
    VkCommandBuffer           commandBuffer,
    const VkRenderingInfoKHR* pRenderingInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BeginRendering(pRenderingInfo);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndRendering(
    VkCommandBuffer           commandBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->EndRendering();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetCullMode(
    VkCommandBuffer                             commandBuffer,
    VkCullModeFlags                             cullMode)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetCullModeEXT(cullMode);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetFrontFace(
    VkCommandBuffer                             commandBuffer,
    VkFrontFace                                 frontFace)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetFrontFaceEXT(frontFace);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveTopology(
    VkCommandBuffer                             commandBuffer,
    VkPrimitiveTopology                         primitiveTopology)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetPrimitiveTopologyEXT(primitiveTopology);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetViewportWithCount(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    viewportCount,
    const VkViewport*                           pViewports)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetViewportWithCount(viewportCount, pViewports);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetScissorWithCount(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    scissorCount,
    const VkRect2D*                             pScissors)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetScissorWithCount(scissorCount, pScissors);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers2(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    firstBinding,
    uint32_t                                    bindingCount,
    const VkBuffer*                             pBuffers,
    const VkDeviceSize*                         pOffsets,
    const VkDeviceSize*                         pSizes,
    const VkDeviceSize*                         pStrides)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BindVertexBuffers(firstBinding,
                                                                     bindingCount,
                                                                     pBuffers,
                                                                     pOffsets,
                                                                     pSizes,
                                                                     pStrides);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthTestEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthTestEnableEXT(depthTestEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthWriteEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthWriteEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthWriteEnableEXT(depthWriteEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthCompareOp(
    VkCommandBuffer                             commandBuffer,
    VkCompareOp                                 depthCompareOp)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthCompareOpEXT(depthCompareOp);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBoundsTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBoundsTestEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthBoundsTestEnableEXT(depthBoundsTestEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilTestEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    stencilTestEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetStencilTestEnableEXT(stencilTestEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilOp(
    VkCommandBuffer                             commandBuffer,
    VkStencilFaceFlags                          faceMask,
    VkStencilOp                                 failOp,
    VkStencilOp                                 passOp,
    VkStencilOp                                 depthFailOp,
    VkCompareOp                                 compareOp)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetStencilOpEXT(faceMask, failOp, passOp, depthFailOp, compareOp);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorBuffersEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    bufferCount,
    const VkDescriptorBufferBindingInfoEXT*     pBindingInfos)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BindDescriptorBuffers(
        bufferCount,
        pBindingInfos);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDescriptorBufferOffsetsEXT(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    setCount,
    const uint32_t*                             pBufferIndices,
    const VkDeviceSize*                         pOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDescriptorBufferOffsets(
        pipelineBindPoint,
        layout,
        firstSet,
        setCount,
        pBufferIndices,
        pOffsets);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorBufferEmbeddedSamplersEXT(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BindDescriptorBufferEmbeddedSamplers(
        pipelineBindPoint,
        layout,
        set);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorWriteEnableEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    attachmentCount,
    const VkBool32*                             pColorWriteEnables)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetColorWriteEnableEXT(attachmentCount, pColorWriteEnables);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizerDiscardEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    rasterizerDiscardEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetRasterizerDiscardEnableEXT(rasterizerDiscardEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetPrimitiveRestartEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    primitiveRestartEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetPrimitiveRestartEnableEXT(primitiveRestartEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBiasEnable(
    VkCommandBuffer                             commandBuffer,
    VkBool32                                    depthBiasEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthBiasEnableEXT(depthBiasEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLogicOpEXT(
    VkCommandBuffer                             commandBuffer,
    VkLogicOp                                   logicOp)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetLogicOp(logicOp);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetPatchControlPointsEXT(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    patchControlPoints)
{
    VK_NOT_IMPLEMENTED;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage2(
    VkCommandBuffer                             commandBuffer,
    const VkBlitImageInfo2KHR*                  pBlitImageInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->BlitImage(
        pBlitImageInfo->srcImage,
        pBlitImageInfo->srcImageLayout,
        pBlitImageInfo->dstImage,
        pBlitImageInfo->dstImageLayout,
        pBlitImageInfo->regionCount,
        pBlitImageInfo->pRegions,
        pBlitImageInfo->filter);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyBufferInfo2KHR*                 pCopyBufferInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyBuffer(
        pCopyBufferInfo->srcBuffer,
        pCopyBufferInfo->dstBuffer,
        pCopyBufferInfo->regionCount,
        pCopyBufferInfo->pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyBufferToImageInfo2KHR*          pCopyBufferToImageInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyBufferToImage(
        pCopyBufferToImageInfo->srcBuffer,
        pCopyBufferToImageInfo->dstImage,
        pCopyBufferToImageInfo->dstImageLayout,
        pCopyBufferToImageInfo->regionCount,
        pCopyBufferToImageInfo->pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyImageInfo2KHR*                  pCopyImageInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyImage(
        pCopyImageInfo->srcImage,
        pCopyImageInfo->srcImageLayout,
        pCopyImageInfo->dstImage,
        pCopyImageInfo->dstImageLayout,
        pCopyImageInfo->regionCount,
        pCopyImageInfo->pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer2(
    VkCommandBuffer                             commandBuffer,
    const VkCopyImageToBufferInfo2KHR*          pCopyImageToBufferInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->CopyImageToBuffer(
        pCopyImageToBufferInfo->srcImage,
        pCopyImageToBufferInfo->srcImageLayout,
        pCopyImageToBufferInfo->dstBuffer,
        pCopyImageToBufferInfo->regionCount,
        pCopyImageToBufferInfo->pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage2(
    VkCommandBuffer                             commandBuffer,
    const VkResolveImageInfo2KHR*               pResolveImageInfo)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->ResolveImage(
        pResolveImageInfo->srcImage,
        pResolveImageInfo->srcImageLayout,
        pResolveImageInfo->dstImage,
        pResolveImageInfo->dstImageLayout,
        pResolveImageInfo->regionCount,
        pResolveImageInfo->pRegions);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->VkDevice()->GetEntryPoints().vkCmdPushDescriptorSetKHR(
        commandBuffer,
        pipelineBindPoint,
        layout,
        set,
        descriptorWriteCount,
        pDescriptorWrites);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer                             commandBuffer,
    VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    const void*                                 pData)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->VkDevice()->GetEntryPoints().vkCmdPushDescriptorSetWithTemplateKHR(
        commandBuffer,
        descriptorUpdateTemplate,
        layout,
        set,
        pData);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetTessellationDomainOriginEXT(
    VkCommandBuffer                     commandBuffer,
    VkTessellationDomainOrigin          domainOrigin)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetTessellationDomainOrigin(domainOrigin);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClampEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            depthClampEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthClampEnable(depthClampEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetPolygonModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkPolygonMode                       polygonMode)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetPolygonMode(polygonMode);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizationSamplesEXT(
    VkCommandBuffer                     commandBuffer,
    VkSampleCountFlagBits               rasterizationSamples)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetRasterizationSamples(rasterizationSamples);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleMaskEXT(
    VkCommandBuffer                     commandBuffer,
    VkSampleCountFlagBits               samples,
    const VkSampleMask*                 pSampleMask)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetSampleMask(samples, pSampleMask);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetAlphaToCoverageEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            alphaToCoverageEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetAlphaToCoverageEnable(alphaToCoverageEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetAlphaToOneEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            alphaToOneEnable)
{
    VK_NOT_IMPLEMENTED;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLogicOpEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            logicOpEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetLogicOpEnable(logicOpEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendEnableEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkBool32*                     pColorBlendEnables)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetColorBlendEnable(firstAttachment,
                                                                       attachmentCount,
                                                                       pColorBlendEnables);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendEquationEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkColorBlendEquationEXT*      pColorBlendEquations)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetColorBlendEquation(firstAttachment,
                                                                         attachmentCount,
                                                                            pColorBlendEquations);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorWriteMaskEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const  VkColorComponentFlags*       pColorWriteMasks)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetColorWriteMask(firstAttachment,
                                                                     attachmentCount,
                                                                     pColorWriteMasks);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetRasterizationStreamEXT(
    VkCommandBuffer                     commandBuffer,
    uint32_t                            rasterizationStream)
{
    VK_NOT_IMPLEMENTED;
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetConservativeRasterizationModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkConservativeRasterizationModeEXT  conservativeRasterizationMode)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetConservativeRasterizationMode(conservativeRasterizationMode);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetExtraPrimitiveOverestimationSizeEXT(
    VkCommandBuffer                     commandBuffer,
    float                               extraPrimitiveOverestimationSize)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->
        SetExtraPrimitiveOverestimationSize(extraPrimitiveOverestimationSize);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClipEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            depthClipEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthClipEnable(depthClipEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetSampleLocationsEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            sampleLocationsEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetSampleLocationsEnable(sampleLocationsEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetColorBlendAdvancedEXT(
    VkCommandBuffer                      commandBuffer,
    uint32_t                             firstAttachment,
    uint32_t                             attachmentCount,
    const VkColorBlendAdvancedEXT*       pColorBlendAdvanced)
{
    VK_NEVER_CALLED();
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetProvokingVertexModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkProvokingVertexModeEXT            provokingVertexMode)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetProvokingVertexMode(provokingVertexMode);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineRasterizationModeEXT(
    VkCommandBuffer                     commandBuffer,
    VkLineRasterizationModeEXT          lineRasterizationMode)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetLineRasterizationMode(lineRasterizationMode);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetLineStippleEnableEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            stippledLineEnable)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetLineStippleEnable(stippledLineEnable);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthClipNegativeOneToOneEXT(
    VkCommandBuffer                     commandBuffer,
    VkBool32                            negativeOneToOne)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDepthClipNegativeOneToOne(negativeOneToOne);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdSetVertexInputEXT(
    VkCommandBuffer                              commandBuffer,
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetVertexInput(vertexBindingDescriptionCount,
                                                                  pVertexBindingDescriptions,
                                                                  vertexAttributeDescriptionCount,
                                                                  pVertexAttributeDescriptions);

}

} // namespace entry

} // namespace vk
