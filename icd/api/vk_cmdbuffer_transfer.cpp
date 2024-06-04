/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_image.h"
#include "include/vk_query.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_acceleration_structure.h"
#include "raytrace/ray_tracing_device.h"
#endif

#include "sqtt/sqtt_mgr.h"

namespace vk
{

// =====================================================================================================================
void CmdBuffer::PalCmdCopyBuffer(
    Buffer*                pSrcBuffer,
    Buffer*                pDstBuffer,
    uint32_t               regionCount,
    Pal::MemoryCopyRegion* pRegions)
{
    if (m_pDevice->IsMultiGpu() == false)
    {
        Pal::IGpuMemory* const pSrcMemory = pSrcBuffer->PalMemory(DefaultDeviceIndex);
        Pal::IGpuMemory* const pDstMemory = pDstBuffer->PalMemory(DefaultDeviceIndex);
        VK_ASSERT(pSrcMemory != nullptr);
        VK_ASSERT(pDstMemory != nullptr);

        PalCmdBuffer(DefaultDeviceIndex)->CmdCopyMemory(
            *pSrcMemory,
            *pDstMemory,
            regionCount,
            pRegions);
    }
    else
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyMemory(
                *pSrcBuffer->PalMemory(deviceIdx),
                *pDstBuffer->PalMemory(deviceIdx),
                regionCount,
                pRegions);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdUpdateBuffer(
    Buffer*         pDestBuffer,
    Pal::gpusize    offset,
    Pal::gpusize    size,
    const uint32_t* pData)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdUpdateMemory(*pDestBuffer->PalMemory(deviceIdx), offset, size, pData);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdFillBuffer(
    Buffer*         pDestBuffer,
    Pal::gpusize    offset,
    Pal::gpusize    size,
    uint32_t        data)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdFillMemory(*pDestBuffer->PalMemory(deviceIdx), offset, size, data);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdCopyImage(
    const Image* const    pSrcImage,
    VkImageLayout         srcImageLayout,
    const Image* const    pDstImage,
    VkImageLayout         destImageLayout,
    uint32_t              regionCount,
    Pal::ImageCopyRegion* pRegions)
{
    if ((((pSrcImage->GetImageSamples() == pDstImage->GetImageSamples()) && (pSrcImage->GetImageSamples() > 1)) ||
         (pSrcImage->GetImageType() != pDstImage->GetImageType())) &&
        (m_palQueueType == Pal::QueueType::QueueTypeDma))
    {
        SwitchToBackupCmdBuffer();
    }

    // Convert src/dest VkImageLayouts to PAL types here because we may have just switched to backup command buffer.
    Pal::ImageLayout palSrcImageLayout = pSrcImage->GetBarrierPolicy().GetTransferLayout(
        srcImageLayout, GetQueueFamilyIndex());
    Pal::ImageLayout palDstImageLayout = pDstImage->GetBarrierPolicy().GetTransferLayout(
        destImageLayout, GetQueueFamilyIndex());

    if (m_pDevice->IsMultiGpu() == false)
    {
        PalCmdBuffer(DefaultDeviceIndex)->CmdCopyImage(
            *pSrcImage->PalImage(DefaultDeviceIndex),
            palSrcImageLayout,
            *pDstImage->PalImage(DefaultDeviceIndex),
            palDstImageLayout,
            regionCount,
            pRegions,
            nullptr,
            0);
    }
    else
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyImage(
                *pSrcImage->PalImage(deviceIdx),
                palSrcImageLayout,
                *pDstImage->PalImage(deviceIdx),
                palDstImageLayout,
                regionCount,
                pRegions,
                nullptr,
                0);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdScaledCopyImage(
    const Image* const   pSrcImage,
    const Image* const   pDstImage,
    Pal::ScaledCopyInfo& copyInfo)
{
    if (m_pDevice->IsMultiGpu() == false)
    {
        copyInfo.pSrcImage = pSrcImage->PalImage(DefaultDeviceIndex);
        copyInfo.pDstImage = pDstImage->PalImage(DefaultDeviceIndex);

        // This will do a scaled blit
        PalCmdBuffer(DefaultDeviceIndex)->CmdScaledCopyImage(copyInfo);
    }
    else
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            copyInfo.pSrcImage = pSrcImage->PalImage(deviceIdx);
            copyInfo.pDstImage = pDstImage->PalImage(deviceIdx);

            // This will do a scaled blit
            PalCmdBuffer(deviceIdx)->CmdScaledCopyImage(copyInfo);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdCopyMemoryToImage(
    const Buffer*               pSrcBuffer,
    const Image*                pDstImage,
    Pal::ImageLayout            layout,
    uint32_t                    regionCount,
    Pal::MemoryImageCopyRegion* pRegions)
{
    if (m_pDevice->IsMultiGpu() == false)
    {
        PalCmdBuffer(DefaultDeviceIndex)->CmdCopyMemoryToImage(
            *pSrcBuffer->PalMemory(DefaultDeviceIndex),
            *pDstImage->PalImage(DefaultDeviceIndex),
            layout,
            regionCount,
            pRegions);
    }
    else
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyMemoryToImage(
                *pSrcBuffer->PalMemory(deviceIdx),
                *pDstImage->PalImage(deviceIdx),
                layout,
                regionCount,
                pRegions);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdCopyImageToMemory(
    const Image*                pSrcImage,
    const Buffer*               pDstBuffer,
    Pal::ImageLayout            layout,
    uint32_t                    regionCount,
    Pal::MemoryImageCopyRegion* pRegions)
{
    if (m_pDevice->IsMultiGpu() == false)
    {
        PalCmdBuffer(DefaultDeviceIndex)->CmdCopyImageToMemory(
            *pSrcImage->PalImage(DefaultDeviceIndex),
            layout,
            *pDstBuffer->PalMemory(DefaultDeviceIndex),
            regionCount,
            pRegions);
    }
    else
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyImageToMemory(
                *pSrcImage->PalImage(deviceIdx),
                layout,
                *pDstBuffer->PalMemory(deviceIdx),
                regionCount,
                pRegions);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
template<typename BufferCopyType>
void CmdBuffer::CopyBuffer(
    VkBuffer                                    srcBuffer,
    VkBuffer                                    destBuffer,
    uint32_t                                    regionCount,
    const BufferCopyType*                       pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer);

    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions));
    auto       regionBatch = Util::Min(regionCount, maxRegions);

    // Allocate space to store memory copy regions
    Pal::MemoryCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::MemoryCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        Buffer* pSrcBuffer = Buffer::ObjectFromHandle(srcBuffer);
        Buffer* pDstBuffer = Buffer::ObjectFromHandle(destBuffer);

        for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
        {
            regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

            for (uint32_t i = 0; i < regionBatch; ++i)
            {
                pPalRegions[i].srcOffset    = pSrcBuffer->MemOffset() + pRegions[regionIdx + i].srcOffset;
                pPalRegions[i].dstOffset    = pDstBuffer->MemOffset() + pRegions[regionIdx + i].dstOffset;
                pPalRegions[i].copySize     = pRegions[regionIdx + i].size;
            }

            PalCmdCopyBuffer(pSrcBuffer, pDstBuffer, regionBatch, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer);
}

// =====================================================================================================================
template<typename ImageCopyType>
void CmdBuffer::CopyImage(
    VkImage              srcImage,
    VkImageLayout        srcImageLayout,
    VkImage              destImage,
    VkImageLayout        destImageLayout,
    uint32_t             regionCount,
    const ImageCopyType* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyImage);

    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions)), MaxPalAspectsPerMask);
    auto       regionBatch = Util::Min(regionCount * MaxPalAspectsPerMask, maxRegions);

    Pal::ImageCopyRegion* pPalRegions =
        virtStackFrame.AllocArray<Pal::ImageCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage    = Image::ObjectFromHandle(srcImage);
        const Image* const pDstImage    = Image::ObjectFromHandle(destImage);

        const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat(), m_pDevice->GetRuntimeSettings());
        const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat(), m_pDevice->GetRuntimeSettings());

        for (uint32_t regionIdx = 0; regionIdx < regionCount;)
        {
            uint32_t palRegionCount = 0;

            while ((regionIdx < regionCount) &&
                   (palRegionCount <= (regionBatch - MaxPalAspectsPerMask)))
            {
                VkToPalImageCopyRegion(pRegions[regionIdx], srcFormat.format,
                    pSrcImage->GetArraySize(), dstFormat.format, pDstImage->GetArraySize(), pPalRegions,
                    &palRegionCount);

                ++regionIdx;
            }

            PalCmdCopyImage(pSrcImage, srcImageLayout, pDstImage, destImageLayout, palRegionCount, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyImage);
}

// =====================================================================================================================
template<typename ImageBlitType>
void CmdBuffer::BlitImage(
    VkImage              srcImage,
    VkImageLayout        srcImageLayout,
    VkImage              destImage,
    VkImageLayout        destImageLayout,
    uint32_t             regionCount,
    const ImageBlitType* pRegions,
    VkFilter             filter)
{
    DbgBarrierPreCmd(DbgBarrierCopyImage);

    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions)), MaxPalAspectsPerMask);
    auto       regionBatch = Util::Min(regionCount * MaxPalAspectsPerMask, maxRegions);

    // Allocate space to store scaled image copy regions (we need a separate region per PAL aspect)
    Pal::ImageScaledCopyRegion* pPalRegions =
        virtStackFrame.AllocArray<Pal::ImageScaledCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage    = Image::ObjectFromHandle(srcImage);
        const Image* const pDstImage    = Image::ObjectFromHandle(destImage);

        const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat(), m_pDevice->GetRuntimeSettings());
        const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat(), m_pDevice->GetRuntimeSettings());

        Pal::ScaledCopyInfo palCopyInfo = {};

        palCopyInfo.srcImageLayout = pSrcImage->GetBarrierPolicy().GetTransferLayout(
            srcImageLayout, GetQueueFamilyIndex());
        palCopyInfo.dstImageLayout = pDstImage->GetBarrierPolicy().GetTransferLayout(
            destImageLayout, GetQueueFamilyIndex());

        // Maps blit filters to their PAL equivalent
        palCopyInfo.filter   = VkToPalTexFilter(VK_FALSE, filter, filter, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        palCopyInfo.rotation = Pal::ImageRotation::Ccw0;

        palCopyInfo.pRegions        = pPalRegions;

        // PAL does gamma correction whenever the destination is a SRGB image or treated as one.
        // If the source image is an UNORM image that contains SRGB data, we need to set dstAsNorm
        // so PAL doesn't end up doing gamma correction on values that are already in SRGB space.
        if (pSrcImage->TreatAsSrgb())
        {
            palCopyInfo.flags.dstAsNorm = true;
        }
        else if (pDstImage->TreatAsSrgb())
        {
            palCopyInfo.flags.dstAsSrgb = true;
        }

        for (uint32_t regionIdx = 0; regionIdx < regionCount;)
        {
            palCopyInfo.regionCount = 0;

            // Attempt a lightweight copy image instead of the requested scaled blit.
            const ImageBlitType& region    = pRegions[regionIdx];
            const VkExtent3D     srcExtent =
            {
                static_cast<uint32_t>(region.srcOffsets[1].x - region.srcOffsets[0].x),
                static_cast<uint32_t>(region.srcOffsets[1].y - region.srcOffsets[0].y),
                static_cast<uint32_t>(region.srcOffsets[1].z - region.srcOffsets[0].z)
            };

            if ((pSrcImage->GetFormat() == pDstImage->GetFormat()) &&
                (srcExtent.width  == static_cast<uint32_t>(region.dstOffsets[1].x - region.dstOffsets[0].x)) &&
                (srcExtent.height == static_cast<uint32_t>(region.dstOffsets[1].y - region.dstOffsets[0].y)) &&
                (srcExtent.depth  == static_cast<uint32_t>(region.dstOffsets[1].z - region.dstOffsets[0].z)))
            {
                const VkImageCopy imageCopy =
                {
                    region.srcSubresource,
                    region.srcOffsets[0],
                    region.dstSubresource,
                    region.dstOffsets[0],
                    srcExtent
                };

                Pal::ImageCopyRegion palRegions[MaxPalAspectsPerMask];
                uint32_t             palRegionCount = 0;

                VkToPalImageCopyRegion(imageCopy, srcFormat.format, pSrcImage->GetArraySize(), dstFormat.format,
                    pDstImage->GetArraySize(), palRegions, &palRegionCount);

                PalCmdCopyImage(pSrcImage, srcImageLayout, pDstImage, destImageLayout,
                    palRegionCount, palRegions);

                ++regionIdx;
            }
            else
            {
                while ((regionIdx < regionCount) &&
                       (palCopyInfo.regionCount <= (regionBatch - MaxPalAspectsPerMask)))
                {
                    VkToPalImageScaledCopyRegion(pRegions[regionIdx], srcFormat.format, pSrcImage->GetArraySize(),
                        dstFormat.format, pPalRegions, &palCopyInfo.regionCount);

                    ++regionIdx;
                }

                // This will do a scaled blit
                PalCmdScaledCopyImage(pSrcImage, pDstImage, palCopyInfo);
            }
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyImage);
}

// =====================================================================================================================
// Copies from a buffer of linear data to a region of an image (vkCopyBufferToImage)
template<typename BufferImageCopyType>
void CmdBuffer::CopyBufferToImage(
    VkBuffer                   srcBuffer,
    VkImage                    destImage,
    VkImageLayout              destImageLayout,
    uint32_t                   regionCount,
    const BufferImageCopyType* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);

    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions));
    auto       regionBatch = Util::Min(regionCount, maxRegions);

    // Allocate space to store memory image copy regions
    Pal::MemoryImageCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::MemoryImageCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Buffer* pSrcBuffer         = Buffer::ObjectFromHandle(srcBuffer);
        const Pal::gpusize srcMemOffset  = pSrcBuffer->MemOffset();
        const Image* pDstImage           = Image::ObjectFromHandle(destImage);

        const Pal::ImageLayout layout = pDstImage->GetBarrierPolicy().GetTransferLayout(
            destImageLayout, GetQueueFamilyIndex());

        for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
        {
            regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

            for (uint32_t i = 0; i < regionBatch; ++i)
            {
                // For image-buffer copies we have to override the format for depth-only and stencil-only copies
                Pal::SwizzledFormat dstFormat = VkToPalFormat(
                    Formats::GetAspectFormat(
                    pDstImage->GetFormat(),
                    pRegions[regionIdx + i].imageSubresource.aspectMask),
                    m_pDevice->GetRuntimeSettings());

                uint32 plane =  VkToPalImagePlaneSingle(pDstImage->GetFormat(),
                    pRegions[regionIdx + i].imageSubresource.aspectMask, m_pDevice->GetRuntimeSettings());

                pPalRegions[i] = VkToPalMemoryImageCopyRegion(pRegions[regionIdx + i], dstFormat.format, plane,
                    pDstImage->GetArraySize(), srcMemOffset);
            }

            PalCmdCopyMemoryToImage(pSrcBuffer, pDstImage, layout, regionBatch, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);
}

// =====================================================================================================================
// Copies and detiles a region of an image to a buffer (vkCopyImageToBuffer)
template<typename BufferImageCopyType>
void CmdBuffer::CopyImageToBuffer(
    VkImage                    srcImage,
    VkImageLayout              srcImageLayout,
    VkBuffer                   destBuffer,
    uint32_t                   regionCount,
    const BufferImageCopyType* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);

    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions));
    auto       regionBatch = Util::Min(regionCount, maxRegions);

    // Allocate space to store memory image copy regions
    Pal::MemoryImageCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::MemoryImageCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage      = Image::ObjectFromHandle(srcImage);
        Buffer* pDstBuffer                = Buffer::ObjectFromHandle(destBuffer);
        const Pal::gpusize dstMemOffset   = pDstBuffer->MemOffset();

        const Pal::ImageLayout layout = pSrcImage->GetBarrierPolicy().GetTransferLayout(
            srcImageLayout, GetQueueFamilyIndex());

        for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
        {
            regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

            for (uint32_t i = 0; i < regionBatch; ++i)
            {
                // For image-buffer copies we have to override the format for depth-only and stencil-only copies
                Pal::SwizzledFormat srcFormat = VkToPalFormat(Formats::GetAspectFormat(pSrcImage->GetFormat(),
                    pRegions[regionIdx + i].imageSubresource.aspectMask), m_pDevice->GetRuntimeSettings());

                uint32 plane = VkToPalImagePlaneSingle(pSrcImage->GetFormat(),
                    pRegions[regionIdx + i].imageSubresource.aspectMask, m_pDevice->GetRuntimeSettings());

                pPalRegions[i] = VkToPalMemoryImageCopyRegion(pRegions[regionIdx + i], srcFormat.format, plane,
                    pSrcImage->GetArraySize(), dstMemOffset);
            }

            PalCmdCopyImageToMemory(pSrcImage, pDstBuffer, layout, regionBatch, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);
}

// =====================================================================================================================
void CmdBuffer::UpdateBuffer(
    VkBuffer        destBuffer,
    VkDeviceSize    destOffset,
    VkDeviceSize    dataSize,
    const uint32_t* pData)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer);

    PalCmdSuspendPredication(true);

    Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    PalCmdUpdateBuffer(pDestBuffer, pDestBuffer->MemOffset() + destOffset, dataSize, pData);

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer);
}

// =====================================================================================================================
void CmdBuffer::FillBuffer(
    VkBuffer                                    destBuffer,
    VkDeviceSize                                destOffset,
    VkDeviceSize                                fillSize,
    uint32_t                                    data)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer);

    PalCmdSuspendPredication(true);

    Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    if (fillSize == VK_WHOLE_SIZE)
    {
        fillSize = Util::RoundDownToMultiple(pDestBuffer->GetSize() - destOffset,
                                             static_cast<VkDeviceSize>(sizeof(data) ) );
    }

    PalCmdFillBuffer(pDestBuffer, pDestBuffer->MemOffset() + destOffset, fillSize, data);

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer);
}

// =====================================================================================================================
void CmdBuffer::CopyQueryPoolResults(
    VkQueryPool        queryPool,
    uint32_t           firstQuery,
    uint32_t           queryCount,
    VkBuffer           destBuffer,
    VkDeviceSize       destOffset,
    VkDeviceSize       destStride,
    VkQueryResultFlags flags)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer | DbgBarrierCopyQueryPool);

    PalCmdSuspendPredication(true);

    const QueryPool* pBasePool = QueryPool::ObjectFromHandle(queryPool);
    const Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    if ((pBasePool->GetQueryType() != VK_QUERY_TYPE_TIMESTAMP)
#if VKI_RAY_TRACING
        && (IsAccelerationStructureQueryType(pBasePool->GetQueryType()) == false)
#endif
        )
    {
        const PalQueryPool* pPool = pBasePool->AsPalQueryPool();
        Pal::QueryResultFlags palFlags = VkToPalQueryResultFlags(flags);
        if (pBasePool->GetQueryType() == VK_QUERY_TYPE_PRIMITIVES_GENERATED_EXT)
        {
            palFlags = static_cast<Pal::QueryResultFlags>(
                static_cast<uint32_t>(palFlags) | static_cast<uint32_t>(Pal::QueryResultOnlyPrimNeeded));
        }

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdResolveQuery(
                *pPool->PalPool(deviceIdx),
                palFlags,
                pPool->PalQueryType(),
                firstQuery,
                queryCount,
                *pDestBuffer->PalMemory(deviceIdx),
                pDestBuffer->MemOffset() + destOffset,
                destStride);
        } while (deviceGroup.IterateNext());
    }
    else
    {
        QueryCopy(
            pBasePool,
            pDestBuffer,
            firstQuery,
            queryCount,
            destOffset,
            destStride,
            flags);
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer | DbgBarrierCopyQueryPool);
}

// ===================================================================================================================
// Command to write a timestamp value to a location in a Timestamp query pool
void CmdBuffer::QueryCopy(
    const QueryPool* pBasePool,
    const Buffer* pDestBuffer,
    uint32_t           firstQuery,
    uint32_t           queryCount,
    VkDeviceSize       destOffset,
    VkDeviceSize       destStride,
    VkQueryResultFlags flags)
{
    const QueryPoolWithStorageView* pPool = pBasePool->AsQueryPoolWithStorageView();

    const Device::InternalPipeline& pipeline =
#if VKI_RAY_TRACING
    (IsAccelerationStructureSerializationType(pBasePool->GetQueryType())) ?
        m_pDevice->GetInternalAccelerationStructureQueryCopyPipeline() :
#endif
        m_pDevice->GetTimestampQueryCopyPipeline();

    // Wait for all previous query timestamps to complete.  For now we have to do a full pipeline idle but once
    // we have a PAL interface for doing a 64-bit WAIT_REG_MEM, we only have to wait on the queries being copied
    // here
    if ((flags & VK_QUERY_RESULT_WAIT_BIT) != 0)
    {
        static const Pal::BarrierTransition transition =
        {
            pBasePool->GetQueryType() == VK_QUERY_TYPE_TIMESTAMP ? Pal::CoherTimestamp : Pal::CoherMemory,
            Pal::CoherShaderRead
        };

        static const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;

        static const Pal::BarrierInfo WriteWaitIdle =
        {
            Pal::HwPipePreCs,                               // waitPoint
            1,                                              // pipePointWaitCount
            &pipePoint,                                     // pPipePoints
            0,                                              // gpuEventWaitCount
            nullptr,                                        // ppGpuEvents
            0,                                              // rangeCheckedTargetWaitCount
            nullptr,                                        // ppTargets
            1,                                              // transitionCount
            &transition,                                    // pTransitions
            0,                                              // globalSrcCacheMask
            0,                                              // globalDstCacheMask
            RgpBarrierInternalPreCopyQueryPoolResultsSync   // reason
        };

        PalCmdBarrier(WriteWaitIdle, m_curDeviceMask);
    }

    uint32_t userData[16];

    // Figure out which user data registers should contain what compute constants
    const uint32_t storageViewSize = m_pDevice->GetProperties().descriptorSizes.bufferView;
    const uint32_t storageViewDwSize = storageViewSize / sizeof(uint32_t);
    const uint32_t viewOffset = 0;
    const uint32_t bufferViewOffset = storageViewDwSize;
    const uint32_t queryCountOffset = bufferViewOffset + storageViewDwSize;
    const uint32_t copyFlagsOffset = queryCountOffset + 1;
    const uint32_t copyStrideOffset = copyFlagsOffset + 1;
    const uint32_t firstQueryOffset = copyStrideOffset + 1;
    const uint32_t ptrQueryOffset = firstQueryOffset + 1;
    const uint32_t userDataCount = ptrQueryOffset + 1;

    // Make sure they agree with pipeline mapping
    VK_ASSERT(viewOffset == pipeline.userDataNodeOffsets[0]);
    VK_ASSERT(bufferViewOffset == pipeline.userDataNodeOffsets[1]);
    VK_ASSERT(queryCountOffset == pipeline.userDataNodeOffsets[2]);
    VK_ASSERT(userDataCount <= VK_ARRAY_SIZE(userData));

    // Create and set a raw storage view into the destination buffer (shader will choose to either write 32-bit or
    // 64-bit values)
    Pal::BufferViewInfo bufferViewInfo = {};

    bufferViewInfo.range           = destStride * queryCount;
    bufferViewInfo.stride          = 0; // Raw buffers have a zero byte stride
    bufferViewInfo.swizzledFormat  = Pal::UndefinedSwizzledFormat;

    // Set query count
    userData[queryCountOffset] = queryCount;

    // These are magic numbers that match literal values in the shader
    constexpr uint32_t Copy64Bit = 0x1;
    constexpr uint32_t CopyIncludeAvailabilityBit = 0x2;

    // Set copy flags
    userData[copyFlagsOffset] = 0;
    userData[copyFlagsOffset] |= (flags & VK_QUERY_RESULT_64_BIT) ? Copy64Bit : 0x0;
    userData[copyFlagsOffset] |= (flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? CopyIncludeAvailabilityBit : 0x0;

    // Set destination stride
    VK_ASSERT(destStride <= UINT_MAX); // TODO: Do we really need to handle this?

    userData[copyStrideOffset] = static_cast<uint32_t>(destStride);

    // Set start query index
    userData[firstQueryOffset] = firstQuery;

#if VKI_RAY_TRACING
    // Set the acceleration structure query offset
    userData[ptrQueryOffset] =
        (pBasePool->GetQueryType() == VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR) ?
        0x1 : 0x0;
#endif

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // Backup PAL compute state
        PalCmdBuffer(deviceIdx)->CmdSaveComputeState(Pal::ComputeStatePipelineAndUserData);

        Pal::PipelineBindParams bindParams = {};
        bindParams.pipelineBindPoint = Pal::PipelineBindPoint::Compute;
        bindParams.pPipeline = pipeline.pPipeline[deviceIdx];
        bindParams.apiPsoHash = Pal::InternalApiPsoHash;

        // Bind the copy compute pipeline
        PalCmdBuffer(deviceIdx)->CmdBindPipeline(bindParams);

        // Set the query buffer SRD (copy source) as typed 64-bit storage view
        memcpy(&userData[viewOffset], pPool->GetStorageView(deviceIdx), storageViewSize);

        bufferViewInfo.gpuAddr = pDestBuffer->GpuVirtAddr(deviceIdx) + destOffset;
        m_pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(1, &bufferViewInfo, &userData[bufferViewOffset]);

        // Write user data registers
        PalCmdBuffer(deviceIdx)->CmdSetUserData(
            Pal::PipelineBindPoint::Compute,
            0,
            userDataCount,
            userData);

        // Figure out how many thread groups we need to dispatch and dispatch
        constexpr uint32_t ThreadsPerGroup = 64;

        uint32_t threadGroupCount = Util::Max(1U, (queryCount + ThreadsPerGroup - 1) / ThreadsPerGroup);

        PalCmdBuffer(deviceIdx)->CmdDispatch({ threadGroupCount, 1, 1 });

        // Restore compute state
        PalCmdBuffer(deviceIdx)->CmdRestoreComputeState(Pal::ComputeStatePipelineAndUserData);

        // Note that the application is responsible for doing a post-copy sync using a barrier.
    } while (deviceGroup.IterateNext());
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void CmdBuffer::CopyAccelerationStructure(
    const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        CopyAccelerationStructurePerDevice(
            deviceIdx,
            pInfo);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::CopyAccelerationStructurePerDevice(
    const uint32_t                              deviceIdx,
    const VkCopyAccelerationStructureInfoKHR*   pInfo)
{
    // Only valid modes for AS-AS copy
    VK_ASSERT((pInfo->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR) ||
              (pInfo->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_CLONE_KHR));

    const AccelerationStructure* pDst   = AccelerationStructure::ObjectFromHandle(pInfo->dst);
    const AccelerationStructure* pSrc   = AccelerationStructure::ObjectFromHandle(pInfo->src);
    GpuRt::AccelStructCopyInfo copyInfo = {};

    copyInfo.mode                       = AccelerationStructure::ConvertCopyAccelerationStructureModeKHR(pInfo->mode);
    copyInfo.dstAccelStructAddr.gpu     = (pDst != nullptr) ? pDst->GetDeviceAddress(deviceIdx) : 0;
    copyInfo.srcAccelStructAddr.gpu     = (pSrc != nullptr) ? pSrc->GetDeviceAddress(deviceIdx) : 0;

    m_pDevice->RayTrace()->GpuRt(deviceIdx)->CopyAccelStruct(PalCmdBuffer(deviceIdx), copyInfo);
}

// =====================================================================================================================
void CmdBuffer::CopyAccelerationStructureToMemory(
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        CopyAccelerationStructureToMemoryPerDevice(
            deviceIdx,
            pInfo);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::CopyAccelerationStructureToMemoryPerDevice(
    const uint32_t                                    deviceIndex,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    // Only valid mode
    VK_ASSERT(pInfo->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_SERIALIZE_KHR);

    const AccelerationStructure* pSrc   = AccelerationStructure::ObjectFromHandle(pInfo->src);
    GpuRt::AccelStructCopyInfo copyInfo = {};

    copyInfo.mode = AccelerationStructure::ConvertCopyAccelerationStructureModeKHR(pInfo->mode);

    copyInfo.srcAccelStructAddr.gpu = (pSrc != nullptr) ? pSrc->GetDeviceAddress(deviceIndex) : 0;
    copyInfo.dstAccelStructAddr.gpu = pInfo->dst.deviceAddress;

    m_pDevice->RayTrace()->GpuRt(deviceIndex)->CopyAccelStruct(PalCmdBuffer(deviceIndex), copyInfo);
}

// =====================================================================================================================
void CmdBuffer::CopyMemoryToAccelerationStructure(
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    // Only valid mode
    VK_ASSERT(pInfo->mode == VK_COPY_ACCELERATION_STRUCTURE_MODE_DESERIALIZE_KHR);

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        CopyMemoryToAccelerationStructurePerDevice(
            deviceIdx,
            pInfo);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::CopyMemoryToAccelerationStructurePerDevice(
    const uint32_t                                      deviceIndex,
    const VkCopyMemoryToAccelerationStructureInfoKHR*   pInfo)
{
    GpuRt::AccelStructCopyInfo copyInfo = {};

    copyInfo.mode = AccelerationStructure::ConvertCopyAccelerationStructureModeKHR(pInfo->mode);

    const AccelerationStructure* pDst = AccelerationStructure::ObjectFromHandle(pInfo->dst);

    copyInfo.srcAccelStructAddr.gpu = pInfo->src.deviceAddress;
    copyInfo.dstAccelStructAddr.gpu = (pDst != nullptr) ? pDst->GetDeviceAddress(deviceIndex) : 0;

    m_pDevice->RayTrace()->GpuRt(deviceIndex)->CopyAccelStruct(PalCmdBuffer(deviceIndex), copyInfo);
}

#endif

// =====================================================================================================================
// Template instantiation needed for references entry.cpp.

template
void CmdBuffer::BlitImage<VkImageBlit>(
    VkImage                 srcImage,
    VkImageLayout           srcImageLayout,
    VkImage                 destImage,
    VkImageLayout           destImageLayout,
    uint32_t                regionCount,
    const VkImageBlit*      pRegions,
    VkFilter                filter);

template
void CmdBuffer::BlitImage<VkImageBlit2>(
    VkImage                 srcImage,
    VkImageLayout           srcImageLayout,
    VkImage                 destImage,
    VkImageLayout           destImageLayout,
    uint32_t                regionCount,
    const VkImageBlit2*     pRegions,
    VkFilter                filter);

template
void CmdBuffer::CopyBuffer<VkBufferCopy>(
    VkBuffer                srcBuffer,
    VkBuffer                destBuffer,
    uint32_t                regionCount,
    const VkBufferCopy*     pRegions);

template
void CmdBuffer::CopyBuffer<VkBufferCopy2>(
    VkBuffer                srcBuffer,
    VkBuffer                destBuffer,
    uint32_t                regionCount,
    const VkBufferCopy2*    pRegions);

template
void CmdBuffer::CopyBufferToImage<VkBufferImageCopy>(
    VkBuffer                    srcBuffer,
    VkImage                     destImage,
    VkImageLayout               destImageLayout,
    uint32_t                    regionCount,
    const VkBufferImageCopy*    pRegions);

template
void CmdBuffer::CopyBufferToImage<VkBufferImageCopy2>(
    VkBuffer                    srcBuffer,
    VkImage                     destImage,
    VkImageLayout               destImageLayout,
    uint32_t                    regionCount,
    const VkBufferImageCopy2*    pRegions);

template
void CmdBuffer::CopyImage<VkImageCopy>(
    VkImage             srcImage,
    VkImageLayout       srcImageLayout,
    VkImage             destImage,
    VkImageLayout       destImageLayout,
    uint32_t            regionCount,
    const VkImageCopy*  pRegions);

template
void CmdBuffer::CopyImage<VkImageCopy2>(
    VkImage             srcImage,
    VkImageLayout       srcImageLayout,
    VkImage             destImage,
    VkImageLayout       destImageLayout,
    uint32_t            regionCount,
    const VkImageCopy2* pRegions);

template
void CmdBuffer::CopyImageToBuffer<VkBufferImageCopy>(
    VkImage                     srcImage,
    VkImageLayout               srcImageLayout,
    VkBuffer                    destBuffer,
    uint32_t                    regionCount,
    const VkBufferImageCopy*    pRegions);

template
void CmdBuffer::CopyImageToBuffer<VkBufferImageCopy2>(
    VkImage                     srcImage,
    VkImageLayout               srcImageLayout,
    VkBuffer                    destBuffer,
    uint32_t                    regionCount,
    const VkBufferImageCopy2*   pRegions);

} // namespace vk
