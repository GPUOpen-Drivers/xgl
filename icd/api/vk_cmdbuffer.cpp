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

#include "include/vk_buffer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_compute_pipeline.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_descriptor_set.h"
#include "include/vk_descriptor_update_template.h"
#include "include/vk_event.h"
#include "include/vk_formats.h"
#include "include/vk_framebuffer.h"
#include "include/vk_image_view.h"
#include "include/vk_render_pass.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_physical_device.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_image.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"
#include "include/vk_query.h"
#include "include/vk_queue.h"

#if VKI_RAY_TRACING
#include "raytrace/vk_acceleration_structure.h"
#include "raytrace/vk_ray_tracing_pipeline.h"
#include "raytrace/ray_tracing_device.h"
#include "raytrace/ray_tracing_util.h"
#include "gpurt/gpurtLib.h"
#include "gpurt/gpurtCounter.h"
#endif

#include "sqtt/sqtt_layer.h"
#include "sqtt/sqtt_mgr.h"

#include "palCmdBuffer.h"
#include "palFormatInfo.h"
#include "palGpuEvent.h"
#include "palImage.h"
#include "palQueryPool.h"
#include "palSysMemory.h"
#include "palDevice.h"
#include "palGpuUtil.h"
#include "palFormatInfo.h"
#include "palVectorImpl.h"
#include "palAutoBuffer.h"

#include <float.h>

#if ICD_GPUOPEN_DEVMODE_BUILD
#include "devmode/devmode_mgr.h"
#endif

namespace vk
{
namespace
{
// =====================================================================================================================
// Creates a compatible PAL "clear box" structure from attachment + render area for a renderpass clear.
Pal::Box BuildClearBox(
    const Pal::Rect&               renderArea,
    const Framebuffer::Attachment& attachment)
{
    Pal::Box box { };

    // 2D area
    box.offset.x      = renderArea.offset.x;
    box.offset.y      = renderArea.offset.y;
    box.extent.width  = renderArea.extent.width;
    box.extent.height = renderArea.extent.height;

    if (attachment.pImage->GetImageType() == VK_IMAGE_TYPE_3D)
    {
        if (attachment.pImage->Is2dArrayCompatible())
        {
            box.offset.z     = attachment.zRange.offset;
            box.extent.depth = attachment.zRange.extent;
        }
        else
        {
            // Whole slice range (these are offset relative to subresrange)
            box.offset.z     = attachment.subresRange[0].startSubres.arraySlice;
            box.extent.depth = attachment.subresRange[0].numSlices;
        }
    }
    else
    {
        box.offset.z     = 0;
        box.extent.depth = 1;
    }

    return box;
}

// =====================================================================================================================
// Creates a compatible PAL "clear box" structure from attachment + render area for a renderpass clear.
Pal::Box BuildClearBox(
    const Pal::Rect& renderArea,
    const ImageView& imageView)
{
    Pal::Box box{ };

    // 2D area
    box.offset.x      = renderArea.offset.x;
    box.offset.y      = renderArea.offset.y;
    box.extent.width  = renderArea.extent.width;
    box.extent.height = renderArea.extent.height;

    // Get the attachment image
    const Image* pImage = imageView.GetImage();

    if (pImage->GetImageType() == VK_IMAGE_TYPE_3D)
    {
        if (pImage->Is2dArrayCompatible())
        {
            box.offset.z     = imageView.GetZRange().offset;
            box.extent.depth = imageView.GetZRange().extent;
        }
        else
        {
            Pal::SubresRange subresRange;
            imageView.GetFrameBufferAttachmentSubresRange(&subresRange);

            // Whole slice range (these are offset relative to subresrange)
            box.offset.z     = subresRange.startSubres.arraySlice;
            box.extent.depth = subresRange.numSlices;
        }
    }
    else
    {
        box.offset.z     = 0;
        box.extent.depth = 1;
    }

    return box;
}

// =====================================================================================================================
// Returns ranges of consecutive bits set to 1 from a bit mask.
//
// uint32 { 0xE47F01D6 } -> [(1, 2) (4, 1) (6, 3) (16, 7) (26, 1) (29, 3)]
//
// <----->   <->     <------------->             <-----> <-> <--->
// +---------------------------------------------------------------+
// |1 1 1 0 0 1 0 0 0 1 1 1 1 1 1 1 0 0 0 0 0 0 0 1 1 1 0 1 0 1 1 0|
// +---------------------------------------------------------------+
//
// @note The implementation of RangesOfOnesInBitMask() assumes that bitMask ends with 0.
//       To satisfy that condition, the bitMask is promoted to uint64_t,
//       filled with leading zeros and looped through only relevant 33 bits.
//       Mentioned assumption allows avoiding edge case
//       in which bitMask ends in the middle of range of ones.
//
Util::Vector<Pal::Range, 16, Util::GenericAllocator> RangesOfOnesInBitMask(
    const uint32_t bitMask)
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Util::Vector<Pal::Range, 16, Util::GenericAllocator> rangesOfOnes { nullptr };

    constexpr int32_t INVALID_INDEX = -1;
    int32_t rangeStart = INVALID_INDEX;

    for (int32_t bitIndex = 0; bitIndex <= 32; ++bitIndex)
    {
        const bool bitValue = (bitMask & (uint64_t { 0x1 } << bitIndex)) > 0;

        if (bitValue) // 1
        {
            if (rangeStart == INVALID_INDEX)
            {
                rangeStart = bitIndex;
            }
        }
        else // 0
        {
            if (rangeStart != INVALID_INDEX)
            {
                const uint32_t rangeLength = bitIndex - rangeStart;
                rangesOfOnes.PushBack(Pal::Range { rangeStart, rangeLength });

                rangeStart = INVALID_INDEX;
            }
        }
    }

    return rangesOfOnes;
}

// =====================================================================================================================
// Populate a vector with PAL clear regions converted from Vulkan clear rects.
// If multiview is enabled layer ranges are overridden according to viewMask.
// Returns Pal::Result::Success if completed successfully.
template <typename PalClearRegionVect>
Pal::Result CreateClearRegions (
    const uint32_t              rectCount,
    const VkClearRect* const    pRects,
    const uint32_t              viewMask,
    const uint32_t              zOffset,
    PalClearRegionVect* const   pOutClearRegions)
{
    using ClearRegionType = typename std::remove_pointer<decltype(pOutClearRegions->Data())>::type;

    static_assert(std::is_same<ClearRegionType, Pal::ClearBoundTargetRegion>::value ||
                  std::is_same<ClearRegionType, Pal::Box>::value, "Wrong element type");
    VK_ASSERT(pOutClearRegions != nullptr);

    Pal::Result palResult = Pal::Result::Success;

    pOutClearRegions->Clear();

    if (viewMask > 0)
    {
        const auto layerRanges = RangesOfOnesInBitMask(viewMask);

        palResult = pOutClearRegions->Reserve(rectCount * layerRanges.NumElements());

        if (palResult == Pal::Result::Success)
        {
            for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
            {
                for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
                {
                    pOutClearRegions->PushBack(VkToPalClearRegion<ClearRegionType>(pRects[rectIndex], zOffset));
                    OverrideLayerRanges(pOutClearRegions->Back(), layerRangeIt.Get());
                }
            }
        }
    }
    else
    {
        palResult = pOutClearRegions->Reserve(rectCount);

        if (palResult == Pal::Result::Success)
        {
            for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
            {
                pOutClearRegions->PushBack(VkToPalClearRegion<ClearRegionType>(pRects[rectIndex], zOffset));
            }
        }
    }

    return palResult;
}

// =====================================================================================================================
// Populate a vector with attachment's PAL subresource ranges defined by clearInfo with modified layer ranges
// according to Vulkan clear rects (multiview disabled) or viewMask (multiview is enabled).
// Returns Pal::Result::Success if completed successfully.
template<typename PalSubresRangeVector>
Pal::Result CreateClearSubresRanges(
    const vk::ImageView*            pImageView,
    const VkClearAttachment&        clearInfo,
    const uint32_t                  rectCount,
    const VkClearRect* const        pRects,
    const uint32_t                  viewMask,
    PalSubresRangeVector* const     pOutClearSubresRanges)
{
    static_assert(std::is_same<decltype(pOutClearSubresRanges->Data()), Pal::SubresRange*>::value, "Wrong element type");
    VK_ASSERT(pOutClearSubresRanges != nullptr);

    Pal::Result palResult = Pal::Result::Success;

    Pal::SubresRange subresRange = {};
    pImageView->GetFrameBufferAttachmentSubresRange(&subresRange);

    pOutClearSubresRanges->Clear();

    bool hasPlaneDepthAndStencil = false;

    if (pImageView->GetImage()->HasStencil() && pImageView->GetImage()->HasDepth())
    {
        if (clearInfo.aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT)
        {
            subresRange.startSubres.plane = 1;
        }
        else
        {
            hasPlaneDepthAndStencil = (clearInfo.aspectMask ==
                                      (VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT));
        }
    }

    if (viewMask > 0)
    {
        const auto layerRanges = RangesOfOnesInBitMask(viewMask);

        palResult = pOutClearSubresRanges->Reserve(layerRanges.NumElements() *(hasPlaneDepthAndStencil ? 2 : 1));

        if (palResult == Pal::Result::Success)
        {
            for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
            {
                pOutClearSubresRanges->PushBack(subresRange);
                pOutClearSubresRanges->Back().startSubres.arraySlice += layerRangeIt.Get().offset;
                pOutClearSubresRanges->Back().numSlices = layerRangeIt.Get().extent;

                if (hasPlaneDepthAndStencil)
                {
                    subresRange.startSubres.plane = 1;
                    pOutClearSubresRanges->PushBack(subresRange);
                    pOutClearSubresRanges->Back().startSubres.arraySlice += layerRangeIt.Get().offset;
                    pOutClearSubresRanges->Back().numSlices = layerRangeIt.Get().extent;
                }
            }
        }
    }
    else
    {
         palResult = pOutClearSubresRanges->Reserve(rectCount *(hasPlaneDepthAndStencil ? 2 : 1));

         if (palResult == Pal::Result::Success)
         {
            for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
            {
                pOutClearSubresRanges->PushBack(subresRange);
                pOutClearSubresRanges->Back().startSubres.arraySlice += pRects[rectIndex].baseArrayLayer;
                pOutClearSubresRanges->Back().numSlices = pRects[rectIndex].layerCount;

                if (hasPlaneDepthAndStencil)
                {
                    subresRange.startSubres.plane = 1;
                    pOutClearSubresRanges->PushBack(subresRange);
                    pOutClearSubresRanges->Back().startSubres.arraySlice += pRects[rectIndex].baseArrayLayer;
                    pOutClearSubresRanges->Back().numSlices = pRects[rectIndex].layerCount;
                }
            }
         }
    }

    return palResult;
}

// =====================================================================================================================
// Populate a vector with attachment's PAL subresource ranges defined by clearInfo with modified layer ranges
// according to Vulkan clear rects (multiview disabled) or viewMask (multiview is enabled).
// Returns Pal::Result::Success if completed successfully.
template<typename PalSubresRangeVector>
Pal::Result CreateClearSubresRanges(
    const Framebuffer::Attachment&  attachment,
    const VkClearAttachment&        clearInfo,
    const uint32_t                  rectCount,
    const VkClearRect* const        pRects,
    const RenderPass&               renderPass,
    const uint32_t                  subpass,
    PalSubresRangeVector* const     pOutClearSubresRanges)
{
    static_assert(std::is_same<decltype(pOutClearSubresRanges->Data()), Pal::SubresRange*>::value, "Wrong element type");
    VK_ASSERT(pOutClearSubresRanges != nullptr);

    Pal::Result palResult              = Pal::Result::Success;
    const auto  attachmentSubresRanges = attachment.FindSubresRanges(clearInfo.aspectMask);

    pOutClearSubresRanges->Clear();

    if (renderPass.IsMultiviewEnabled())
    {
        const auto viewMask    = renderPass.GetViewMask(subpass);
        const auto layerRanges = RangesOfOnesInBitMask(viewMask);

        palResult = pOutClearSubresRanges->Reserve(attachmentSubresRanges.NumElements() * layerRanges.NumElements());

        if (palResult == Pal::Result::Success)
        {
            for (uint32_t rangeIndex = 0; rangeIndex < attachmentSubresRanges.NumElements(); ++rangeIndex)
            {
                for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
                {
                    pOutClearSubresRanges->PushBack(attachmentSubresRanges.At(rangeIndex));
                    pOutClearSubresRanges->Back().startSubres.arraySlice += layerRangeIt.Get().offset;
                    pOutClearSubresRanges->Back().numSlices               = layerRangeIt.Get().extent;
                }
            }
        }
    }
    else
    {
        palResult = pOutClearSubresRanges->Reserve(attachmentSubresRanges.NumElements() * rectCount);

        if (palResult == Pal::Result::Success)
        {
            for (uint32_t rangeIndex = 0; rangeIndex < attachmentSubresRanges.NumElements(); ++rangeIndex)
            {
                for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
                {
                    pOutClearSubresRanges->PushBack(attachmentSubresRanges.At(rangeIndex));
                    pOutClearSubresRanges->Back().startSubres.arraySlice += pRects[rectIndex].baseArrayLayer;
                    pOutClearSubresRanges->Back().numSlices               = pRects[rectIndex].layerCount;
                }
            }
        }
    }

    return palResult;
}

// =====================================================================================================================
// Returns attachment's PAL subresource ranges defined by clearInfo for LoadOp Clear.
// When multiview is enabled, layer ranges are modified according active views during a renderpass.
Util::Vector<Pal::SubresRange, MaxPalAspectsPerMask * Pal::MaxViewInstanceCount, Util::GenericAllocator>
LoadOpClearSubresRanges(
    const Framebuffer::Attachment& attachment,
    const RPLoadOpClearInfo&       clearInfo,
    const RenderPass&              renderPass)
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Util::Vector<Pal::SubresRange, MaxPalAspectsPerMask * Pal::MaxViewInstanceCount, Util::GenericAllocator> clearSubresRanges { nullptr };

    const auto attachmentSubresRanges = attachment.FindSubresRanges(clearInfo.aspect);

    if (renderPass.IsMultiviewEnabled())
    {
        const auto activeViews = renderPass.GetActiveViewsBitMask();
        const auto layerRanges = RangesOfOnesInBitMask(activeViews);

        for (uint32_t rangeIndex = 0; rangeIndex < attachmentSubresRanges.NumElements(); ++rangeIndex)
        {
            for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
            {
                clearSubresRanges.PushBack(attachmentSubresRanges.At(rangeIndex));
                clearSubresRanges.Back().startSubres.arraySlice += layerRangeIt.Get().offset;
                clearSubresRanges.Back().numSlices               = layerRangeIt.Get().extent;
            }
        }
    }
    else
    {
        for (uint32_t rangeIndex = 0; rangeIndex < attachmentSubresRanges.NumElements(); ++rangeIndex)
        {
            clearSubresRanges.PushBack(attachmentSubresRanges.At(rangeIndex));
        }
    }

    return clearSubresRanges;
}

// =====================================================================================================================
// Populate a vector with PAL rects created from Vulkan clear rects.
// Returns Pal::Result::Success if completed successfully.
template<typename PalRectVector>
Pal::Result CreateClearRects(
    const uint32_t              rectCount,
    const VkClearRect* const    pRects,
    PalRectVector* const        pOutClearRects)
{
    static_assert(std::is_same<decltype(pOutClearRects->Data()), Pal::Rect*>::value, "Wrong element type");
    VK_ASSERT(pOutClearRects != nullptr);

    pOutClearRects->Clear();

    const auto palResult = pOutClearRects->Reserve(rectCount);

    if (palResult == Pal::Result::Success)
    {
        for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
        {
            pOutClearRects->PushBack(VkToPalRect(pRects[rectIndex].rect));
        }
    }

    return palResult;
}

} // anonymous ns

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    Device*                         pDevice,
    CmdPool*                        pCmdPool,
    uint32_t                        queueFamilyIndex)
    :
    m_pDevice(pDevice),
    m_pCmdPool(pCmdPool),
    m_queueFamilyIndex(queueFamilyIndex),
    m_palQueueType(pDevice->GetQueueFamilyPalQueueType(queueFamilyIndex)),
    m_palEngineType(pDevice->GetQueueFamilyPalEngineType(queueFamilyIndex)),
    m_curDeviceMask(0),
    m_rpDeviceMask(0),
    m_cbBeginDeviceMask(0),
    m_numPalDevices(pDevice->NumPalDevices()),
    m_validShaderStageFlags(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetValidShaderStages(queueFamilyIndex)),
    m_pStackAllocator(nullptr),
    m_allGpuState { },
    m_flags(),
    m_recordingResult(VK_SUCCESS),
    m_pSqttState(nullptr),
    m_renderPassInstance(pDevice->VkInstance()->Allocator()),
    m_pTransformFeedbackState(nullptr),
    m_palDepthStencilState(pDevice->VkInstance()->Allocator()),
    m_palColorBlendState(pDevice->VkInstance()->Allocator()),
    m_palMsaaState(pDevice->VkInstance()->Allocator()),
    m_uberFetchShaderInternalDataMap(8, pDevice->VkInstance()->Allocator()),
    m_pUberFetchShaderTempBuffer(nullptr),
    m_debugPrintf(pDevice->VkInstance()->Allocator()),
    m_reverseThreadGroupState(false)
#if VKI_RAY_TRACING
    , m_rayTracingIndirectList(pDevice->VkInstance()->Allocator())
#endif
{
    m_flags.wasBegun = false;

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    m_optimizeCmdbufMode             = settings.optimizeCmdbufMode;
    m_asyncComputeQueueMaxWavesPerCu = settings.asyncComputeQueueMaxWavesPerCu;

#if VK_ENABLE_DEBUG_BARRIERS
    m_dbgBarrierPreCmdMask  = settings.dbgBarrierPreCmdEnable;
    m_dbgBarrierPostCmdMask = settings.dbgBarrierPostCmdEnable;
#endif

    m_flags.padVertexBuffers                    = settings.padVertexBuffers;
    m_flags.prefetchCommands                    = settings.prefetchCommands;
    m_flags.prefetchShaders                     = settings.prefetchShaders;
    m_flags.disableResetReleaseResources        = settings.disableResetReleaseResources;
    m_flags.subpassLoadOpClearsBoundAttachments = settings.subpassLoadOpClearsBoundAttachments;
    m_flags.preBindDefaultState                 = settings.preBindDefaultState;

    Pal::DeviceProperties info;
    m_pDevice->PalDevice(DefaultDeviceIndex)->GetProperties(&info);

    m_flags.useBackupBuffer = false;
    memset(m_pBackupPalCmdBuffers, 0, sizeof(Pal::ICmdBuffer*) * MaxPalDevices);

    // If supportReleaseAcquireInterface is true, the ASIC provides new barrier interface CmdReleaseThenAcquire()
    // designed for Acquire/Release-based driver. This flag is currently enabled for gfx9 and above.
    // If supportSplitReleaseAcquire is true, the ASIC provides split CmdRelease() and CmdAcquire() to express barrier,
    // and CmdReleaseThenAcquire() is still valid. This flag is currently enabled for gfx10 and above.
    m_flags.useReleaseAcquire       = info.gfxipProperties.flags.supportReleaseAcquireInterface &&
                                      settings.useAcquireReleaseInterface;
    m_flags.useSplitReleaseAcquire  = m_flags.useReleaseAcquire &&
                                      info.gfxipProperties.flags.supportSplitReleaseAcquire;
}

// =====================================================================================================================
// Creates a new Vulkan Command Buffer object
VkResult CmdBuffer::Create(
    Device*                            pDevice,
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer*                   pCommandBuffers)
{
    VK_ASSERT(pAllocateInfo->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);

    // Get information about the Vulkan command buffer
    Pal::CmdBufferCreateInfo palCreateInfo = {};

    CmdPool* pCmdPool                     = CmdPool::ObjectFromHandle(pAllocateInfo->commandPool);
    uint32 queueFamilyIndex               = pCmdPool->GetQueueFamilyIndex();
    uint32 commandBufferCount             = pAllocateInfo->commandBufferCount;
    palCreateInfo.pCmdAllocator           = pCmdPool->PalCmdAllocator(DefaultDeviceIndex);
    palCreateInfo.queueType               = pDevice->GetQueueFamilyPalQueueType(queueFamilyIndex);
    palCreateInfo.engineType              = pDevice->GetQueueFamilyPalEngineType(queueFamilyIndex);
    palCreateInfo.flags.nested            = (pAllocateInfo->level > VK_COMMAND_BUFFER_LEVEL_PRIMARY) ? 1 : 0;
    palCreateInfo.flags.dispatchTunneling = 1;

    // Allocate system memory for the command buffer objects
    Pal::Result palResult;

    const uint32    numGroupedCmdBuffers = pDevice->NumPalDevices();
    const size_t    apiSize              = sizeof(ApiCmdBuffer);
    const size_t    perGpuSize           = sizeof(PerGpuRenderState) * numGroupedCmdBuffers;
    const size_t    palSize              = pDevice->PalDevice(DefaultDeviceIndex)->
                                               GetCmdBufferSize(palCreateInfo, &palResult) * numGroupedCmdBuffers;
    size_t          inaccessibleSize     = 0;

    // Accumulate the setBindingData size that will not be accessed based on available pipeline bind points.
    {
#if VKI_RAY_TRACING
        static_assert(PipelineBindRayTracing + 1 == PipelineBindCount, "This code relies on the enum order!");

        if (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_RAY_TRACING_PIPELINE) == false)
        {
            inaccessibleSize += sizeof(uint32) * MaxBindingRegCount;

            static_assert(PipelineBindGraphics + 1 == PipelineBindRayTracing, "This code relies on the enum order!");
#else
        {
#endif
            static_assert(PipelineBindCompute + 1 == PipelineBindGraphics, "This code relies on the enum order!");

            if (palCreateInfo.queueType == Pal::QueueType::QueueTypeCompute)
            {
                inaccessibleSize += sizeof(uint32) * MaxBindingRegCount;
            }
        }
    }

    // Accumulate the setBindingData size that will not be accessed based on the dynamic descriptor data size
    inaccessibleSize += (MaxDynDescRegCount -
                         (MaxDynamicDescriptors * DescriptorSetLayout::GetDynamicBufferDescDwSize(pDevice)))
                        * sizeof(uint32);

    // The total object size less any inaccessible setBindingData (for the last device only to not disrupt MGPU indexing)
    size_t cmdBufSize = apiSize + palSize + perGpuSize - inaccessibleSize;

    size_t sizeDesBuf = 0;
    if (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_DESCRIPTOR_BUFFER))
    {
        // Descriptor buffers have a single dedicated bind point.
        sizeDesBuf = sizeof(DescBufBinding);
        cmdBufSize += sizeDesBuf;
    }

    VK_ASSERT(palResult == Pal::Result::Success);

    VkResult result = VK_SUCCESS;

    uint32 allocCount = 0;

    while ((result == VK_SUCCESS) && (allocCount < commandBufferCount))
    {
        // Allocate memory for the command buffer
        void* pMemory = pDevice->AllocApiObject(pCmdPool->GetCmdPoolAllocator(), cmdBufSize);
        // Create the command buffer
        if (pMemory != nullptr)
        {
            void* pPalMem = Util::VoidPtrInc(pMemory, apiSize + perGpuSize - inaccessibleSize);

            VK_INIT_DISPATCHABLE(CmdBuffer, pMemory, (pDevice,
                                                      pCmdPool,
                                                      queueFamilyIndex));

            pCommandBuffers[allocCount] = reinterpret_cast<VkCommandBuffer>(pMemory);

            CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[allocCount]);

            if ((sizeDesBuf != 0) && (result == VK_SUCCESS))
            {
                pCmdBuffer->m_allGpuState.pDescBufBinding = static_cast<DescBufBinding*>(
                                                            Util::VoidPtrInc(pPalMem, palSize));

                memset(pCmdBuffer->m_allGpuState.pDescBufBinding, 0, sizeof(DescBufBinding));
            }
            else
            {
                pCmdBuffer->m_allGpuState.pDescBufBinding = nullptr;
            }

            result = pCmdBuffer->Initialize(pPalMem, palCreateInfo);

            allocCount++;
        }
        else
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (result != VK_SUCCESS)
    {
        // Failed to create at least one command buffer; destroy any command buffers that we did succeed in creating
        for (uint32_t bufIdx = 0; bufIdx < commandBufferCount; ++bufIdx)
        {
            if (bufIdx < allocCount)
            {
                ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[bufIdx])->Destroy();
            }

            // No partial failures allowed for creating multiple command buffers. Update all to VK_NULL_HANDLE.
            pCommandBuffers[bufIdx] = VK_NULL_HANDLE;
        }
    }

    return result;
}

// =====================================================================================================================
// Initializes the command buffer.  Called once during command buffer creation.
VkResult CmdBuffer::Initialize(
    void*                           pPalMem,
    const Pal::CmdBufferCreateInfo& createInfo)
{
    Pal::Result result = Pal::Result::Success;

    Pal::CmdBufferCreateInfo groupCreateInfo = createInfo;

    // Create the PAL command buffers
    size_t       palMemOffset = 0;
    const size_t palSize      = m_pDevice->PalDevice(DefaultDeviceIndex)->GetCmdBufferSize(groupCreateInfo, &result);

    const uint32_t numGroupedCmdBuffers = m_numPalDevices;

    for (uint32_t groupedIdx = 0; (groupedIdx < numGroupedCmdBuffers) && (result == Pal::Result::Success); groupedIdx++)
    {
        Pal::IDevice* const pPalDevice = m_pDevice->PalDevice(groupedIdx);

        groupCreateInfo.pCmdAllocator = m_pCmdPool->PalCmdAllocator(groupedIdx);

        result = pPalDevice->CreateCmdBuffer(
            groupCreateInfo, Util::VoidPtrInc(pPalMem, palMemOffset), &m_pPalCmdBuffers[groupedIdx]);

        if (result == Pal::Result::Success)
        {
            m_pPalCmdBuffers[groupedIdx]->SetClientData(this);
            palMemOffset += palSize;

            VK_ASSERT(palSize == pPalDevice->GetCmdBufferSize(groupCreateInfo, &result));
            VK_ASSERT(result == Pal::Result::Success);
        }
    }

    if (result == Pal::Result::Success)
    {
        InitializeVertexBuffer();
    }

    if (result == Pal::Result::Success)
    {
        // Register this command buffer with the pool
        result = m_pCmdPool->RegisterCmdBuffer(this);
    }

    if (result == Pal::Result::Success)
    {
        m_flags.is2ndLvl = groupCreateInfo.flags.nested;

        m_allGpuState.stencilRefMasks.flags.u8All = 0xff;

        // Set up the default front/back op values == 1
        m_allGpuState.stencilRefMasks.frontOpValue = DefaultStencilOpValue;
        m_allGpuState.stencilRefMasks.backOpValue = DefaultStencilOpValue;

        m_allGpuState.logicOpEnable = VK_FALSE;
        m_allGpuState.logicOp = VK_LOGIC_OP_COPY;
    }

    // Initialize SQTT command buffer state if thread tracing support is enabled (gpuopen developer mode).
    if ((result == Pal::Result::Success) && (m_pDevice->GetSqttMgr() != nullptr))
    {
        void* pSqttStorage = m_pDevice->VkInstance()->AllocMem(sizeof(SqttCmdBufferState),
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSqttStorage != nullptr)
        {
            m_pSqttState = VK_PLACEMENT_NEW(pSqttStorage) SqttCmdBufferState(this);
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    if (result == Pal::Result::Success)
    {
        result = m_uberFetchShaderInternalDataMap.Init();
    }

    if ((result == Pal::Result::Success) && (createInfo.queueType == Pal::QueueType::QueueTypeDma))
    {
        result = BackupInitialize(createInfo);
    }

    if (result == Pal::Result::Success)
    {
        m_debugPrintf.Init(m_pDevice);
    }
    return PalToVkResult(result);
}

// =====================================================================================================================
// Create backup pal cmdbuffer, only call when DMA queue cmdbuffer be created
Pal::Result CmdBuffer::BackupInitialize(
    const Pal::CmdBufferCreateInfo& createInfo)
{
    Pal::Result palResult = Pal::Result::Success;

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->IsComputeEngineSupported() &&
        settings.useBackupCmdbuffer)
    {
        for (uint32_t queuefamilyIdx = 0; queuefamilyIdx < Queue::MaxQueueFamilies; queuefamilyIdx++)
        {
            if (m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalQueueType(queuefamilyIdx)
                == Pal::QueueType::QueueTypeCompute)
            {
                m_backupQueueFamilyIndex = queuefamilyIdx;
                break;
            }
        }

        Pal::CmdBufferCreateInfo palCreateInfo = createInfo;
        const VkAllocationCallbacks* pAllocCB = m_pCmdPool->GetCmdPoolAllocator();

        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            palCreateInfo.pCmdAllocator = m_pCmdPool->PalCmdAllocator(deviceIdx);
            palCreateInfo.queueType = Pal::QueueTypeCompute;
            palCreateInfo.engineType = Pal::EngineTypeCompute;

            Pal::IDevice* const pPalDevice = m_pDevice->PalDevice(deviceIdx);
            const size_t palSize = pPalDevice->GetCmdBufferSize(palCreateInfo, &palResult);

            if (palResult == Pal::Result::Success)
            {
                void* pMemory = pAllocCB->pfnAllocation(pAllocCB->pUserData,
                    palSize,
                    VK_DEFAULT_MEM_ALIGN,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
                if (pMemory != nullptr)
                {
                    palResult = pPalDevice->CreateCmdBuffer(palCreateInfo, pMemory, &m_pBackupPalCmdBuffers[deviceIdx]);

                    if (palResult == Pal::Result::Success)
                    {
                        m_pBackupPalCmdBuffers[deviceIdx]->SetClientData(this);
                    }
                    else
                    {
                        pAllocCB->pfnFree(
                            pAllocCB->pUserData,
                            pMemory);
                        break;
                    }
                }
                else
                {
                    palResult = Pal::Result::ErrorOutOfMemory;
                }
            }
        }

        if (palResult != Pal::Result::Success)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
            {
                if (m_pBackupPalCmdBuffers[deviceIdx] != nullptr)
                {
                    m_pBackupPalCmdBuffers[deviceIdx]->Destroy();
                    pAllocCB->pfnFree(
                        pAllocCB->pUserData,
                        m_pBackupPalCmdBuffers[deviceIdx]);
                }
            }
        }
    }

    return palResult;
}

// =====================================================================================================================
// Will switch to use backupcmdbuffer based on m_flags.useBackupBuffer
void CmdBuffer::SwitchToBackupCmdBuffer()
{
    if ((m_flags.useBackupBuffer == false) && (m_pBackupPalCmdBuffers[0] != nullptr))
    {
        // need to use backupbuffer set the flag
        m_flags.useBackupBuffer = true;
        uint32_t tempQueueFamilyIndex = m_queueFamilyIndex;
        m_queueFamilyIndex = m_backupQueueFamilyIndex;
        m_backupQueueFamilyIndex = tempQueueFamilyIndex;
        m_palQueueType = Pal::QueueType::QueueTypeCompute;
        m_palEngineType = Pal::EngineType::EngineTypeCompute;

        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            constexpr Pal::CmdBufferBuildInfo info = { };
            m_pBackupPalCmdBuffers[deviceIdx]->Begin(info);
            Pal::ICmdBuffer* tempCmdBuffer = m_pBackupPalCmdBuffers[deviceIdx];
            m_pBackupPalCmdBuffers[deviceIdx] = m_pPalCmdBuffers[deviceIdx];
            m_pPalCmdBuffers[deviceIdx] = tempCmdBuffer;
        }
    }
}

// =====================================================================================================================
// Will restored from backupcmdbuffer based on m_flags.useBackupBuffer
void CmdBuffer::RestoreFromBackupCmdBuffer()
{
    if (m_flags.useBackupBuffer)
    {
        // need to use original palcmdbuffer
        uint32_t tempQueueFamilyIndex = m_queueFamilyIndex;
        m_queueFamilyIndex = m_backupQueueFamilyIndex;
        m_backupQueueFamilyIndex = tempQueueFamilyIndex;
        m_palQueueType = Pal::QueueType::QueueTypeDma;
        m_palEngineType = Pal::EngineType::EngineTypeDma;

        for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
        {
            m_pPalCmdBuffers[deviceIdx]->End();
            Pal::ICmdBuffer* tempCmdBuffer = m_pBackupPalCmdBuffers[deviceIdx];
            m_pBackupPalCmdBuffers[deviceIdx] = m_pPalCmdBuffers[deviceIdx];
            m_pPalCmdBuffers[deviceIdx] = tempCmdBuffer;
        }
    }
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferBegin(const Pal::CmdBufferBuildInfo& cmdInfo)
{
    Pal::Result result = Pal::Result::Success;

    utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        result = PalCmdBuffer(deviceIdx)->Begin(cmdInfo);

        VK_ASSERT(result == Pal::Result::Success);

        const Pal::IBorderColorPalette* pPalBorderColorPalette = m_pDevice->GetPalBorderColorPalette(deviceIdx);
        if (pPalBorderColorPalette != nullptr)
        {
            if ((m_palQueueType == Pal::QueueTypeUniversal) || (m_palQueueType == Pal::QueueTypeCompute))
            {
                if (m_palQueueType == Pal::QueueTypeUniversal)
                {
                    // Bind graphics border color palette on universal queue.
                    PalCmdBuffer(deviceIdx)->CmdBindBorderColorPalette(
                        Pal::PipelineBindPoint::Graphics, pPalBorderColorPalette);
                }

                PalCmdBuffer(deviceIdx)->CmdBindBorderColorPalette(
                    Pal::PipelineBindPoint::Compute, pPalBorderColorPalette);
            }
        }
    }
    while (deviceGroup.IterateNext());

    return result;
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferEnd()
{
    Pal::Result result = Pal::Result::Success;

    utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        result = PalCmdBuffer(deviceIdx)->End();

        VK_ASSERT(result == Pal::Result::Success);
    }
    while (deviceGroup.IterateNext());

    return result;
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferReset(bool returnGpuMemory)
{
    Pal::Result result = Pal::Result::Success;

    // If there was no begin, skip the reset
    if (m_cbBeginDeviceMask != 0)
    {
        utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            result = PalCmdBuffer(deviceIdx)->Reset(nullptr, returnGpuMemory);
            VK_ASSERT(result == Pal::Result::Success);
        }
        while (deviceGroup.IterateNext());

        if (returnGpuMemory)
        {
            m_cbBeginDeviceMask = 0;
        }
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::PalCmdBufferDestroy()
{
    for (uint32_t deviceIdx = 0; deviceIdx < VkDevice()->NumPalDevices(); deviceIdx++)
    {
        Pal::ICmdBuffer* pCmdBuffer = PalCmdBuffer(deviceIdx);
        if (pCmdBuffer != nullptr)
        {
            pCmdBuffer->Destroy();
        }
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindIndexData(
    Buffer* pBuffer,
    Pal::gpusize offset,
    Pal::IndexType indexType,
    Pal::gpusize bufferSize)
{
    uint32_t indexCount = 0;
    if (bufferSize == VK_WHOLE_SIZE)
    {
        indexCount = utils::BufferSizeToIndexCount(indexType, pBuffer->GetSize() - offset);
    }
    else
    {
        indexCount = utils::BufferSizeToIndexCount(indexType, bufferSize);
    }

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        const Pal::gpusize gpuVirtAddr = pBuffer->GpuVirtAddr(deviceIdx) + offset;

        PalCmdBuffer(deviceIdx)->CmdBindIndexData(gpuVirtAddr,
            indexCount,
            indexType);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdUnbindIndexData(Pal::IndexType indexType)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdBindIndexData(0, 0, indexType);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDraw(
    uint32_t firstVertex,
    uint32_t vertexCount,
    uint32_t firstInstance,
    uint32_t instanceCount,
    uint32_t drawId)
{
    // Currently only Vulkan graphics pipelines use PAL graphics pipeline bindings so there's no need to
    // add a delayed validation check for graphics.
    VK_ASSERT(PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Graphics, PipelineBindGraphics));

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdDraw(firstVertex,
            vertexCount,
            firstInstance,
            instanceCount,
            drawId);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDrawIndexed(
    uint32_t firstIndex,
    uint32_t indexCount,
    int32_t  vertexOffset,
    uint32_t firstInstance,
    uint32_t instanceCount,
    uint32_t drawId)
{
    // Currently only Vulkan graphics pipelines use PAL graphics pipeline bindings so there's no need to
    // add a delayed validation check for graphics.
    VK_ASSERT(PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Graphics, PipelineBindGraphics));

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdDrawIndexed(firstIndex,
            indexCount,
            vertexOffset,
            firstInstance,
            instanceCount,
            drawId);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDrawMeshTasks(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdDispatchMesh({ x, y, z });
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
template<bool useBufferCount>
void CmdBuffer::PalCmdDrawMeshTasksIndirect(
    VkBuffer     buffer,
    VkDeviceSize offset,
    uint32_t     count,
    uint32_t     stride,
    VkBuffer     countBuffer,
    VkDeviceSize countOffset)
{
    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    // The indirect argument should be in the range of the given buffer size
    VK_ASSERT((stride + offset) <= pBuffer->PalMemory(DefaultDeviceIndex)->Desc().size);

    const Pal::gpusize paramOffset = pBuffer->MemOffset() + offset;
    Pal::gpusize countVirtAddr = 0;

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        if (useBufferCount)
        {
            Buffer* pCountBuffer = Buffer::ObjectFromHandle(countBuffer);
            countVirtAddr = pCountBuffer->GpuVirtAddr(deviceIdx) + countOffset;
        }

        PalCmdBuffer(deviceIdx)->CmdDispatchMeshIndirectMulti(
            *pBuffer->PalMemory(deviceIdx),
            paramOffset,
            stride,
            count,
            countVirtAddr);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDispatch(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdDispatch({ x, y, z });
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDispatchOffset(
    uint32_t base_x,
    uint32_t base_y,
    uint32_t base_z,
    uint32_t size_x,
    uint32_t size_y,
    uint32_t size_z)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdDispatchOffset({ base_x, base_y, base_z },
                                                             { size_x, size_y, size_z },
                                                             { size_x, size_y, size_z });
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdDispatchIndirect(
    Buffer*      pBuffer,
    Pal::gpusize offset)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // TODO use device group dispatch offsets here.
        // Note: check spec to see if offset setting is applications' responsibility.

        PalCmdBuffer(deviceIdx)->CmdDispatchIndirect(
            *pBuffer->PalMemory(deviceIdx),
            pBuffer->MemOffset() + offset);
    }
    while (deviceGroup.IterateNext());
}

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
    if ((pSrcImage->GetImageSamples() == pDstImage->GetImageSamples()) &&
        (pSrcImage->GetImageSamples() > 1) &&
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
// Begin Vulkan command buffer
VkResult CmdBuffer::Begin(
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    VK_ASSERT(pBeginInfo->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    VK_ASSERT(!m_flags.isRecording);

#if VKI_RAY_TRACING
    m_flags.hasRayTracing        = false;
#endif
    m_flags.isRenderingSuspended = false;
    m_flags.wasBegun             = true;

    // Beginning a command buffer implicitly resets its state
    ResetState();

#if VKI_RAY_TRACING
    FreeRayTracingIndirectMemory();
#endif

    const PhysicalDevice*        pPhysicalDevice = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex);
    const Pal::DeviceProperties& deviceProps     = pPhysicalDevice->PalProperties();

    m_flags.useBackupBuffer = false;

    const RuntimeSettings&       settings        = m_pDevice->GetRuntimeSettings();

    Pal::CmdBufferBuildInfo   cmdInfo = {};

    RenderPass*  pRenderPass  = nullptr;
    Framebuffer* pFramebuffer = nullptr;

    const VkCommandBufferInheritanceRenderingInfoKHR* pInheritanceRenderingInfoKHR = nullptr;

    m_cbBeginDeviceMask = m_pDevice->GetPalDeviceMask();

    cmdInfo.flags.u32All = 0;

    // Disabling prefetch on compute queues by default should be better since PAL's prefetch uses DMA_DATA which causes
    // the CP to idle and switch queues on async compute.
    if ((settings.enableAceShaderPrefetch) || (m_palQueueType != Pal::QueueTypeCompute))
    {
        cmdInfo.flags.prefetchCommands = m_flags.prefetchCommands;
        cmdInfo.flags.prefetchShaders  = m_flags.prefetchShaders;
    }

    if (IsProtected())
    {
        cmdInfo.flags.enableTmz = 1;
    }

    Pal::InheritedStateParams inheritedStateParams = {};

    uint32 currentSubPass = 0;

    cmdInfo.flags.optimizeOneTimeSubmit   = (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) ? 1 : 0;
    cmdInfo.flags.optimizeExclusiveSubmit = (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT) ? 0 : 1;

    switch (m_optimizeCmdbufMode)
    {
    case EnableOptimizeForRenderPassContinue:
        cmdInfo.flags.optimizeGpuSmallBatch = (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) ? 1 : 0;
        break;
    case EnableOptimizeCmdbuf:
        cmdInfo.flags.optimizeGpuSmallBatch = 1;
        break;
    case DisableOptimizeCmdbuf:
        cmdInfo.flags.optimizeGpuSmallBatch = 0;
        break;
    default:
        cmdInfo.flags.optimizeGpuSmallBatch = (pBeginInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) ? 1 : 0;
        break;
    }

    if (m_flags.is2ndLvl && (pBeginInfo->pInheritanceInfo != nullptr))
    {
        // Only provide valid inherited state pointer for 2nd level command buffers
        cmdInfo.pInheritedState = &inheritedStateParams;

        pRenderPass     = RenderPass::ObjectFromHandle(pBeginInfo->pInheritanceInfo->renderPass);
        pFramebuffer    = Framebuffer::ObjectFromHandle(pBeginInfo->pInheritanceInfo->framebuffer);
        currentSubPass  = pBeginInfo->pInheritanceInfo->subpass;

        if (pBeginInfo->pInheritanceInfo->occlusionQueryEnable)
        {
            inheritedStateParams.stateFlags.occlusionQuery = 1;
        }

        const void* pNext = pBeginInfo->pInheritanceInfo->pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            if (pHeader->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_CONDITIONAL_RENDERING_INFO_EXT)
            {
                const auto* pExtInfo = static_cast<const VkCommandBufferInheritanceConditionalRenderingInfoEXT*>(pNext);

                inheritedStateParams.stateFlags.predication = pExtInfo->conditionalRenderingEnable;
                m_flags.hasConditionalRendering             = pExtInfo->conditionalRenderingEnable;
            }
            else if (pHeader->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO_KHR)
            {
                VK_ASSERT(m_flags.is2ndLvl);

                pInheritanceRenderingInfoKHR = static_cast<const VkCommandBufferInheritanceRenderingInfoKHR*>(pNext);

                inheritedStateParams.colorTargetCount = pInheritanceRenderingInfoKHR->colorAttachmentCount;
                inheritedStateParams.stateFlags.targetViewState = 1;

                for (uint32_t i = 0; i < inheritedStateParams.colorTargetCount; i++)
                {
                    inheritedStateParams.colorTargetSwizzledFormats[i] =
                        VkToPalFormat(pInheritanceRenderingInfoKHR->pColorAttachmentFormats[i], settings);

                    inheritedStateParams.sampleCount[i] = pInheritanceRenderingInfoKHR->rasterizationSamples;
                }
            }

            pNext = pHeader->pNext;
        }
    }

    const void* pNext = pBeginInfo->pNext;

    while (pNext != nullptr)
    {
        const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<uint32>(pHeader->sType))
        {
            // Convert Vulkan flags to PAL flags.
        case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO:
        {
            const auto* pDeviceGroupInfo = static_cast<const VkDeviceGroupCommandBufferBeginInfo*>(pNext);

            // Check that the application did not set any bits outside of our device group mask.
            VK_ASSERT((m_cbBeginDeviceMask & pDeviceGroupInfo->deviceMask) == pDeviceGroupInfo->deviceMask);

            m_cbBeginDeviceMask &= pDeviceGroupInfo->deviceMask;
            break;
        }

        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }

    m_curDeviceMask = m_cbBeginDeviceMask;

    if (pRenderPass != nullptr) // secondary VkCommandBuffer will be used inside VkRenderPass
    {
        VK_ASSERT(m_flags.is2ndLvl);

        inheritedStateParams.colorTargetCount = pRenderPass->GetSubpassColorReferenceCount(currentSubPass);
        inheritedStateParams.stateFlags.targetViewState = 1;

        for (uint32_t i = 0; i < inheritedStateParams.colorTargetCount; i++)
        {
            inheritedStateParams.colorTargetSwizzledFormats[i] =
                VkToPalFormat(pRenderPass->GetColorAttachmentFormat(currentSubPass, i), settings);
            inheritedStateParams.sampleCount[i] = pRenderPass->GetColorAttachmentSamples(currentSubPass, i);
        }
    }

    Pal::Result result = PalCmdBufferBegin(cmdInfo);

    if (result == Pal::Result::Success)
    {
        result = m_pCmdPool->MarkCmdBufBegun(this);
    }

    if (result == Pal::Result::Success)
    {
        if (m_pStackAllocator == nullptr)
        {
            result = m_pDevice->VkInstance()->StackMgr()->AcquireAllocator(&m_pStackAllocator);
        }
    }

    DbgBarrierPreCmd(DbgBarrierCmdBufStart);

    VK_ASSERT(result == Pal::Result::Success);

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->Begin(pBeginInfo);
    }

    if (result == Pal::Result::Success)
    {
        // If we have to resume an already started render pass then we have to do it here
        if (pRenderPass != nullptr)
        {
            m_allGpuState.pRenderPass = pRenderPass;

            m_renderPassInstance.subpass = currentSubPass;
        }

        if (pInheritanceRenderingInfoKHR != nullptr)
        {
            m_allGpuState.dynamicRenderingInstance.viewMask =
                pInheritanceRenderingInfoKHR->viewMask;

            m_allGpuState.dynamicRenderingInstance.colorAttachmentCount =
                pInheritanceRenderingInfoKHR->colorAttachmentCount;

            for (uint32_t i = 0; i < m_allGpuState.dynamicRenderingInstance.colorAttachmentCount; ++i)
            {
                DynamicRenderingAttachments* pDynamicAttachment =
                    &m_allGpuState.dynamicRenderingInstance.colorAttachments[i];

                pDynamicAttachment->pImageView           = nullptr;
                pDynamicAttachment->attachmentFormat     = pInheritanceRenderingInfoKHR->pColorAttachmentFormats[i];
                pDynamicAttachment->rasterizationSamples = pInheritanceRenderingInfoKHR->rasterizationSamples;
            }

            m_allGpuState.dynamicRenderingInstance.depthAttachment.attachmentFormat     =
                (pInheritanceRenderingInfoKHR->depthAttachmentFormat != VK_FORMAT_UNDEFINED) ?
                pInheritanceRenderingInfoKHR->depthAttachmentFormat :
                pInheritanceRenderingInfoKHR->stencilAttachmentFormat;

            m_allGpuState.dynamicRenderingInstance.depthAttachment.rasterizationSamples =
                pInheritanceRenderingInfoKHR->rasterizationSamples;
        }

        // if input frame buffer object pointer is NULL, it means
        // either this is for a primary command buffer, or this is a secondary command buffer
        // and the command buffer will get the frame buffer object and execution time from
        // beginRenderPass called in the primary command buffer
        if (pFramebuffer != nullptr)
        {
            m_allGpuState.pFramebuffer = pFramebuffer;
        }
    }

    m_flags.isRecording = true;

    if ((pRenderPass != nullptr) || (pInheritanceRenderingInfoKHR != nullptr))
    // secondary VkCommandBuffer will be used inside VkRenderPass
    {
        VK_ASSERT(m_flags.is2ndLvl);

        // In order to use secondary VkCommandBuffer inside VkRenderPass,
        // when vkBeginCommandBuffer() is called, the VkCommandBufferInheritanceInfo
        // has to specify a VkRenderPass, defining VkRenderPasses with which
        // the secondary VkCommandBuffer will be compatible with
        // and a subpass in which that secondary VkCommandBuffer will be used.
        //
        // Note that two compatible VkRenderPasses have to define
        // exactly the same sequence of ViewMasks.
        //
        // Therefore, ViewMask can be retrived from VkRenderPass using subpass
        // and baked into secondary VkCommandBuffer.
        // Vulkan spec guarantees that ViewMask will not have to be updated.
        //
        // Because secondary VkCommandBuffer will be called inside of a VkRenderPass
        // function setting ViewMask for a subpass during the VkRenderPass is called.
        SetViewInstanceMask(GetDeviceMask());
    }

    if (m_palQueueType == Pal::QueueTypeUniversal)
    {
        const VkPhysicalDeviceLimits& limits = pPhysicalDevice->GetLimits();
        Pal::GlobalScissorParams scissorParams = { };
        scissorParams.scissorRegion.extent.width  = limits.maxFramebufferWidth;
        scissorParams.scissorRegion.extent.height = limits.maxFramebufferHeight;
        {
            utils::IterateMask deviceGroup(GetDeviceMask());
            do
            {
                const uint32_t deviceIdx = deviceGroup.Index();
                PalCmdBuffer(deviceIdx)->CmdSetGlobalScissor(scissorParams);
            }
            while (deviceGroup.IterateNext());
        }

        m_allGpuState.staticTokens.pointLineRasterState = DynamicRenderStateToken;
        const Pal::PointLineRasterStateParams params = { DefaultPointSize,
                                                         DefaultLineWidth,
                                                         limits.pointSizeRange[0],
                                                         limits.pointSizeRange[1] };

        utils::IterateMask deviceGroup(GetDeviceMask());
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            PalCmdBuffer(deviceIdx)->CmdSetPointLineRasterState(params);
        }
        while (deviceGroup.IterateNext());

        const uint32_t supportedVrsRates = deviceProps.gfxipProperties.supportedVrsRates;

        // Turn variable rate shading off if it is supported.
        if (supportedVrsRates & (1 << static_cast<uint32_t>(Pal::VrsShadingRate::_1x1)))
        {
            Pal::VrsCenterState centerState = {};
            m_allGpuState.vrsRate     = {};

            m_allGpuState.vrsRate.flags.exposeVrsPixelsMask = 1;

            // Don't use coarse shading.
            m_allGpuState.vrsRate.shadingRate = Pal::VrsShadingRate::_1x1;

            // Set combiner state for for PsIterator and ProvokingVertex
            m_allGpuState.vrsRate.combinerState[static_cast<uint32_t>(Pal::VrsCombinerStage::PsIterSamples)] =
                Pal::VrsCombiner::Override;

            m_allGpuState.vrsRate.combinerState[static_cast<uint32_t>(Pal::VrsCombinerStage::ProvokingVertex)] =
                Pal::VrsCombiner::Override;

            utils::IterateMask deviceGroupVrs(GetDeviceMask());

            do
            {
                const uint32_t deviceIdx = deviceGroupVrs.Index();

                PalCmdBuffer(deviceIdx)->CmdSetVrsCenterState(centerState);

                // A null source image implies 1x1 shading rate for the image combiner stage.
                PalCmdBuffer(deviceIdx)->CmdBindSampleRateImage(nullptr);
            }
            while (deviceGroupVrs.IterateNext());
        }
    }

    // Dirty all the dynamic states, the bit should be cleared with 0 when the corresponding state is
    // static.
    m_allGpuState.dirtyGraphics.u32All = 0xFFFFFFFF;

    if ((m_palQueueType == Pal::QueueTypeUniversal) && m_flags.preBindDefaultState)
    {
        // Set VRS state now to avoid at bind time
        const uint32_t supportedVrsRates = deviceProps.gfxipProperties.supportedVrsRates;

        if (supportedVrsRates & (1 << static_cast<uint32_t>(m_allGpuState.vrsRate.shadingRate)))
        {
            utils::IterateMask deviceGroupVrs(GetDeviceMask());

            do
            {
                const uint32_t deviceIdx = deviceGroupVrs.Index();

                PalCmdBuffer(deviceIdx)->CmdSetPerDrawVrsRate(m_allGpuState.vrsRate);
            }
            while (deviceGroupVrs.IterateNext());
        }

        m_allGpuState.dirtyGraphics.vrs = 0;

        // Set default sample pattern
        m_allGpuState.samplePattern.sampleCount   = 1;
        m_allGpuState.samplePattern.locations     =
            *Device::GetDefaultQuadSamplePattern(m_allGpuState.samplePattern.sampleCount);
        m_allGpuState.sampleLocationsEnable = VK_FALSE;

        PalCmdSetMsaaQuadSamplePattern(m_allGpuState.samplePattern.sampleCount, m_allGpuState.samplePattern.locations);

        m_allGpuState.dirtyGraphics.samplePattern = 0;
    }

    DbgBarrierPostCmd(DbgBarrierCmdBufStart);

    return PalToVkResult(result);
}

// =====================================================================================================================
// End Vulkan command buffer
VkResult CmdBuffer::End(void)
{
    Pal::Result result;

    VK_ASSERT(m_flags.isRecording);

    DbgBarrierPreCmd(DbgBarrierCmdBufEnd);

    // ValidateGraphicsStates tries to update things like viewport or input assembly
    // only cmdBuffers specialized in graphics (universal) are going to use that state
    // other implementations have stub setters with PAL_NEVER_CALLED asserts
    if (m_palQueueType == Pal::QueueTypeUniversal)
    {
        ValidateGraphicsStates();
    }

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->End();
    }

    DbgBarrierPostCmd(DbgBarrierCmdBufEnd);

    RestoreFromBackupCmdBuffer();

    result = PalCmdBufferEnd();

    m_flags.isRecording = false;

    return (m_recordingResult == VK_SUCCESS ? PalToVkResult(result) : m_recordingResult);
}

// =====================================================================================================================
// Resets all state PipelineState.  This function is called both during vkBeginCommandBuffer (inside
// CmdBuffer::ResetState()) and during vkResetCommandBuffer (inside CmdBuffer::ResetState()) and during
// vkExecuteCommands.
void CmdBuffer::ResetPipelineState()
{
    m_allGpuState.boundGraphicsPipelineHash = 0;
    m_allGpuState.pGraphicsPipeline         = nullptr;
    m_allGpuState.pComputePipeline          = nullptr;
#if VKI_RAY_TRACING
    m_allGpuState.pRayTracingPipeline       = nullptr;
#endif

    ResetVertexBuffer();

    // Reset initial static values to "dynamic" values.  This will skip initial redundancy checking because the
    // prior values are unknown.  Since DynamicRenderStateToken is 0, this is covered by the memset above.
    static_assert(DynamicRenderStateToken == 0, "Unexpected value!");
    memset(&m_allGpuState.staticTokens, 0u, sizeof(m_allGpuState.staticTokens));

    memset(&m_allGpuState.depthStencilCreateInfo, 0u, sizeof(m_allGpuState.depthStencilCreateInfo));

    memset(&m_allGpuState.samplePattern, 0u, sizeof(m_allGpuState.samplePattern));

    uint32_t bindIdx = 0;

    do
    {
        memset(&(m_allGpuState.pipelineState[bindIdx].userDataLayout),
            0,
            sizeof(m_allGpuState.pipelineState[bindIdx].userDataLayout));

        m_allGpuState.pipelineState[bindIdx].boundSetCount    = 0;
        m_allGpuState.pipelineState[bindIdx].pushedConstCount = 0;
        m_allGpuState.pipelineState[bindIdx].dynamicBindInfo  = {};
        m_allGpuState.pipelineState[bindIdx].hasDynamicVertexInput = false;
        m_allGpuState.pipelineState[bindIdx].pVertexInputInternalData = nullptr;
        bindIdx++;
    }
    while (bindIdx < PipelineBindCount);

    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    pDynamicState->colorWriteMask = UINT32_MAX;
    pDynamicState->logicOp = Pal::LogicOp::Copy;

    m_allGpuState.colorWriteMask = UINT32_MAX;
    m_allGpuState.colorWriteEnable = UINT32_MAX;
    m_allGpuState.logicOp = VK_LOGIC_OP_COPY;

    // Default MSAA state
    m_allGpuState.msaaCreateInfo.coverageSamples         = 1;
    m_allGpuState.msaaCreateInfo.exposedSamples          = 0;
    m_allGpuState.msaaCreateInfo.pixelShaderSamples      = 1;
    m_allGpuState.msaaCreateInfo.depthStencilSamples     = 1;
    m_allGpuState.msaaCreateInfo.shaderExportMaskSamples = 1;
    m_allGpuState.msaaCreateInfo.sampleMask              = 1;
    m_allGpuState.msaaCreateInfo.sampleClusters          = 1;
    m_allGpuState.msaaCreateInfo.alphaToCoverageSamples  = 1;
    m_allGpuState.msaaCreateInfo.occlusionQuerySamples   = 1;

    m_allGpuState.triangleRasterState.frontFillMode = Pal::FillMode::Solid;
    m_allGpuState.triangleRasterState.backFillMode  = Pal::FillMode::Solid;

    m_allGpuState.palToApiPipeline[uint32_t(Pal::PipelineBindPoint::Compute)]   = PipelineBindCompute;
    m_allGpuState.palToApiPipeline[uint32_t(Pal::PipelineBindPoint::Graphics)]  = PipelineBindGraphics;
    static_assert(VK_ARRAY_SIZE(m_allGpuState.palToApiPipeline) == 2, "PAL PipelineBindPoint not handled");

    const uint32_t numPalDevices = m_numPalDevices;
    uint32_t deviceIdx           = 0;

    do
    {
        PerGpuRenderState* pPerGpuState = PerGpuState(deviceIdx);

        pPerGpuState->pMsaaState                = nullptr;
        pPerGpuState->pColorBlendState          = nullptr;
        pPerGpuState->pDepthStencilState        = nullptr;
        pPerGpuState->scissor.count             = 1;
        pPerGpuState->scissor.scissors[0]       = {};
        pPerGpuState->viewport.count            = 1;
        pPerGpuState->viewport.viewports[0]     = {};
        pPerGpuState->viewport.horzClipRatio    = FLT_MAX;
        pPerGpuState->viewport.vertClipRatio    = FLT_MAX;
        pPerGpuState->viewport.horzDiscardRatio = 1.0f;
        pPerGpuState->viewport.vertDiscardRatio = 1.0f;
        pPerGpuState->viewport.depthRange       = Pal::DepthRange::ZeroToOne;
        pPerGpuState->maxPipelineStackSize      = 0;

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);
}

// =====================================================================================================================
// Resets all state except for the PAL command buffer state.  This function is called both during vkBeginCommandBuffer
// and during vkResetCommandBuffer
void CmdBuffer::ResetState()
{
    // Memset the first section of m_allGpuState.  The second section begins with pipelineState.
    const size_t memsetBytes = offsetof(AllGpuRenderState, pipelineState);

    memset(&m_allGpuState, 0, memsetBytes);

    ResetPipelineState();

    m_curDeviceMask = InvalidPalDeviceMask;

    m_renderPassInstance.pExecuteInfo = nullptr;
    m_renderPassInstance.subpass      = VK_SUBPASS_EXTERNAL;
    m_renderPassInstance.flags.u32All = 0;

    m_recordingResult = VK_SUCCESS;

    m_flags.hasConditionalRendering = false;

#if VKI_RAY_TRACING
#endif

    m_debugPrintf.Reset(m_pDevice);
    if (m_allGpuState.pDescBufBinding != nullptr)
    {
        memset(m_allGpuState.pDescBufBinding, 0, sizeof(DescBufBinding));
    }
}

// =====================================================================================================================
// Reset Vulkan command buffer
VkResult CmdBuffer::Reset(VkCommandBufferResetFlags flags)
{
    VkResult result = VK_SUCCESS;
    bool releaseResources = ((flags & VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != 0);

    if (m_flags.disableResetReleaseResources)
    {
        releaseResources = false;
    }

    if (m_flags.wasBegun || releaseResources)
    {
        // If the command buffer is being recorded, the stack allocator will still be around.
        // Make sure to free it.
        if (m_flags.isRecording)
        {
            End();

            VK_ASSERT(!m_flags.isRecording);
        }

        if (releaseResources)
        {
            ReleaseResources();
        }

#if VKI_RAY_TRACING
        FreeRayTracingIndirectMemory();
#endif

        result = PalToVkResult(PalCmdBufferReset(releaseResources));

        m_flags.wasBegun = false;

        if ((result == VK_SUCCESS) && releaseResources)
        {
            // Notify the command pool that the command buffer is reset.
            m_pCmdPool->UnmarkCmdBufBegun(this);
        }
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::ConvertPipelineBindPoint(
    VkPipelineBindPoint     pipelineBindPoint,
    Pal::PipelineBindPoint* pPalBindPoint,
    PipelineBindPoint*      pApiBind)
{
    switch (pipelineBindPoint)
    {
    case VK_PIPELINE_BIND_POINT_GRAPHICS:
        *pPalBindPoint = Pal::PipelineBindPoint::Graphics;
        *pApiBind      = PipelineBindGraphics;
        break;
    case VK_PIPELINE_BIND_POINT_COMPUTE:
        *pPalBindPoint = Pal::PipelineBindPoint::Compute;
        *pApiBind      = PipelineBindCompute;
        break;
#if VKI_RAY_TRACING
    case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        *pPalBindPoint = Pal::PipelineBindPoint::Compute;
        *pApiBind      = PipelineBindRayTracing;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        *pPalBindPoint = Pal::PipelineBindPoint::Compute;
        *pApiBind      = PipelineBindCompute;
    }
}

// =====================================================================================================================
// Called to rebind a currently bound pipeline of the given type to PAL.  Called from vkCmdBindPipeline() but also from
// various other places when it has been necessary to defer the binding of the pipeline.
//
// This function will also reload user data if necessary because of the pipeline switch.
template<PipelineBindPoint bindPoint, bool fromBindPipeline>
void CmdBuffer::RebindPipeline()
{
    const UserDataLayout* pNewUserDataLayout = nullptr;
    RebindUserDataFlags   rebindFlags = 0;

    Pal::PipelineBindPoint palBindPoint;

    if (bindPoint == PipelineBindCompute)
    {
        const ComputePipeline* pPipeline = m_allGpuState.pComputePipeline;

        if (pPipeline != nullptr)
        {
            const PhysicalDevice*  pPhysicalDevice = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex);

            if ((pPhysicalDevice->GetQueueFamilyPalQueueType(m_queueFamilyIndex) == Pal::QueueTypeCompute) &&
                (m_asyncComputeQueueMaxWavesPerCu > 0))
            {
                Pal::DynamicComputeShaderInfo dynamicInfo = {};

                dynamicInfo.maxWavesPerCu = static_cast<float>(m_asyncComputeQueueMaxWavesPerCu);

                pPipeline->BindToCmdBuffer(this, dynamicInfo);
            }
            else
            {
                pPipeline->BindToCmdBuffer(this, pPipeline->GetBindInfo());
            }

            palBindPoint       = Pal::PipelineBindPoint::Compute;
            pNewUserDataLayout = pPipeline->GetUserDataLayout();
        }
        else
        {
            ComputePipeline::BindNullPipeline(this);
        }

        palBindPoint = Pal::PipelineBindPoint::Compute;
    }
    else if (bindPoint == PipelineBindGraphics)
    {
        const GraphicsPipeline* pPipeline = m_allGpuState.pGraphicsPipeline;

        if (pPipeline != nullptr)
        {
            pPipeline->BindToCmdBuffer(this);

            if (pPipeline->ContainsStaticState(DynamicStatesInternal::VertexInputBindingStride))
            {
                UpdateVertexBufferStrides(pPipeline);
            }

            pNewUserDataLayout = pPipeline->GetUserDataLayout();

            // Update dynamic vertex input state and check whether need rebind uber-fetch shader internal memory
            PipelineBindState* pBindState = &m_allGpuState.pipelineState[PipelineBindGraphics];
            if (pPipeline->ContainsDynamicState(DynamicStatesInternal::VertexInput))
            {
                if (pBindState->hasDynamicVertexInput == false)
                {
                    if (pBindState->pVertexInputInternalData != nullptr)
                    {
                        rebindFlags |= RebindUberFetchInternalMem;
                    }
                    pBindState->hasDynamicVertexInput = true;
                }
                uint32_t newUberFetchShaderUserData = GetUberFetchShaderUserData(pNewUserDataLayout);
                if (GetUberFetchShaderUserData(&pBindState->userDataLayout) != newUberFetchShaderUserData)
                {
                    SetUberFetchShaderUserData(&pBindState->userDataLayout, newUberFetchShaderUserData);

                    if (pBindState->pVertexInputInternalData != nullptr)
                    {
                        rebindFlags |= RebindUberFetchInternalMem;
                    }
                }
            }
            else
            {
                pBindState->hasDynamicVertexInput = false;
            }
        }
        else
        {
            GraphicsPipeline::BindNullPipeline(this);
        }

        palBindPoint = Pal::PipelineBindPoint::Graphics;
    }

#if VKI_RAY_TRACING
    else if (bindPoint == PipelineBindRayTracing)
    {
        const RayTracingPipeline* pPipeline = m_allGpuState.pRayTracingPipeline;

        if (pPipeline != nullptr)
        {
            const PhysicalDevice*  pPhysicalDevice = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex);

            if ((pPhysicalDevice->GetQueueFamilyPalQueueType(m_queueFamilyIndex) == Pal::QueueTypeCompute) &&
                (m_asyncComputeQueueMaxWavesPerCu > 0))
            {
                Pal::DynamicComputeShaderInfo dynamicInfo = {};

                dynamicInfo.maxWavesPerCu = static_cast<float>(m_asyncComputeQueueMaxWavesPerCu);

                pPipeline->BindToCmdBuffer(this, dynamicInfo);
            }
            else
            {
                pPipeline->BindToCmdBuffer(this, pPipeline->GetBindInfo());
            }

            pNewUserDataLayout = pPipeline->GetUserDataLayout();
        }
        else
        {
            RayTracingPipeline::BindNullPipeline(this);
        }

        palBindPoint = Pal::PipelineBindPoint::Compute;
    }
#endif
    else
    {
        VK_NEVER_CALLED();
    }

    // In compact scheme, the top-level user data layout of two compatible pipeline layout may be different.
    // Thus, pipeline layout needs to be checked and rebind the user data if needed.
    // In indirect scheme, the top-level user data layout is always the same for all the pipeline layouts built
    // in this scheme. So user data doesn't require to be rebind in this case.
    // Pipeline layouts in different scheme can never be compatible. In this case, calling vkCmdBindDescriptorSets()
    // to rebind descirptor sets is mandatory for user.
    if ((pNewUserDataLayout->scheme == m_allGpuState.pipelineState[bindPoint].userDataLayout.scheme) &&
        (pNewUserDataLayout->scheme == PipelineLayoutScheme::Compact))
    {
        // Update the current owner of the compute PAL pipeline binding if we bound a pipeline
        if ((fromBindPipeline == false) &&
            (palBindPoint == Pal::PipelineBindPoint::Compute))
        {
            // If the ownership of the PAL binding is changing, the current user data belongs to the old binding and must
            // be reloaded.
            if (PalPipelineBindingOwnedBy(palBindPoint, bindPoint) == false)
            {
                rebindFlags |= RebindUserDataAll;
            }

            m_allGpuState.palToApiPipeline[size_t(Pal::PipelineBindPoint::Compute)] = bindPoint;
        }

        // Graphics pipeline owner should always remain fixed, so we don't have to worry about reloading
        // user data (for that reason) or ownership updates.
        VK_ASSERT(PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Graphics, PipelineBindGraphics));

        // A user data layout switch may also require some user data to be reloaded (for both gfx and compute).
        if (pNewUserDataLayout != nullptr)
        {
            rebindFlags |= SwitchUserDataLayouts(bindPoint, pNewUserDataLayout);
        }
    }

    // Reprogram the user data if necessary
    if (rebindFlags != 0)
    {
        RebindUserData(bindPoint, palBindPoint, rebindFlags);
    }
}

// =====================================================================================================================
// Bind pipeline to command buffer
void CmdBuffer::BindPipeline(
    VkPipelineBindPoint     pipelineBindPoint,
    VkPipeline              pipeline)
{
    DbgBarrierPreCmd(DbgBarrierBindPipeline);

    const Pipeline* pPipeline = Pipeline::BaseObjectFromHandle(pipeline);

    if (pPipeline != nullptr)
    {
#if VKI_RAY_TRACING
        m_flags.hasRayTracing |= pPipeline->HasRayTracing();
#endif

        switch (pipelineBindPoint)
        {
        case VK_PIPELINE_BIND_POINT_COMPUTE:
        {
            if (m_allGpuState.pComputePipeline != pPipeline)
            {
                m_allGpuState.pComputePipeline = static_cast<const ComputePipeline*>(pPipeline);

                if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindCompute))
                {
                    // Defer the binding by invalidating the current PAL compute binding point.  This is because we
                    // don't know what compute-based binding will be utilized until we see the work command.
                    m_allGpuState.palToApiPipeline[size_t(Pal::PipelineBindPoint::Compute)] = PipelineBindCount;
                }
            }

            break;
        }

        case VK_PIPELINE_BIND_POINT_GRAPHICS:
        {
            if (m_allGpuState.pGraphicsPipeline != pPipeline)
            {
                m_allGpuState.pGraphicsPipeline = static_cast<const GraphicsPipeline*>(pPipeline);

                // Can bind the graphics pipeline immediately since only API graphics pipelines use the PAL
                // graphics pipeline.  Note that wave limits may still defer the bind inside RebindPipeline().
                VK_ASSERT(PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Graphics, PipelineBindGraphics));

                RebindPipeline<PipelineBindGraphics, true>();
            }

            break;
        }

#if VKI_RAY_TRACING
        case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        {
            if (m_allGpuState.pRayTracingPipeline != pPipeline)
            {
                m_allGpuState.pRayTracingPipeline = static_cast<const RayTracingPipeline*>(pPipeline);

                if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindRayTracing))
                {
                    // Defer the binding by invalidating the current PAL compute binding point.  This is because we
                    // don't know what compute-based binding will be utilized until we see the work command.
                    m_allGpuState.palToApiPipeline[size_t(Pal::PipelineBindPoint::Compute)] = PipelineBindCount;
                }
            }

            break;
        }
#endif

        default:
            VK_NEVER_CALLED();
            break;
        }
    }

    DbgBarrierPostCmd(DbgBarrierBindPipeline);
}

// =====================================================================================================================
// Called during vkCmdBindPipeline when the new pipeline's layout might be different from the previously bound layout.
// This function will compare the compatibility of those layouts and reprogram any user data to maintain previously-
// written pipeline resources to make them available in the correct locations of the new pipeline layout.
// compatible with the new layout remain correctly bound.
CmdBuffer::RebindUserDataFlags CmdBuffer::SwitchUserDataLayouts(
    PipelineBindPoint      apiBindPoint,
    const UserDataLayout*  pNewUserDataLayout)
{
    VK_ASSERT(pNewUserDataLayout != nullptr);
    VK_ASSERT(pNewUserDataLayout->scheme == PipelineLayoutScheme::Compact);
    VK_ASSERT(m_allGpuState.pipelineState[apiBindPoint].userDataLayout.scheme == PipelineLayoutScheme::Compact);

    PipelineBindState* pBindState = &m_allGpuState.pipelineState[apiBindPoint];

    RebindUserDataFlags flags = 0;

    const auto& newUserDataLayout = pNewUserDataLayout->compact;
    const auto& curUserDataLayout = pBindState->userDataLayout.compact;

    // Rebind descriptor set bindings if necessary
    if ((newUserDataLayout.setBindingRegBase  != curUserDataLayout.setBindingRegBase) |
        (newUserDataLayout.setBindingRegCount != curUserDataLayout.setBindingRegCount))
    {
        flags |= RebindUserDataDescriptorSets;
    }

    // Rebind push constants if necessary
    if ((newUserDataLayout.pushConstRegBase  != curUserDataLayout.pushConstRegBase) |
        (newUserDataLayout.pushConstRegCount != curUserDataLayout.pushConstRegCount))
    {
        flags |= RebindUserDataPushConstants;
    }

    // Cache the new user data layout information
    pBindState->userDataLayout = *pNewUserDataLayout;

    return flags;
}

// =====================================================================================================================
// Called during vkCmdBindPipeline when something requires rebinding API-provided top-level user data (descriptor
// sets, push constants, etc.)
void CmdBuffer::RebindUserData(
    PipelineBindPoint      apiBindPoint,
    Pal::PipelineBindPoint palBindPoint,
    RebindUserDataFlags    flags)
{
    VK_ASSERT(flags != 0);
    VK_ASSERT(m_allGpuState.pipelineState[apiBindPoint].userDataLayout.scheme == PipelineLayoutScheme::Compact);

    const PipelineBindState& bindState = m_allGpuState.pipelineState[apiBindPoint];
    const auto& userDataLayout         = bindState.userDataLayout.compact;

    if ((flags & RebindUserDataDescriptorSets) != 0)
    {
        const uint32_t count = Util::Min(userDataLayout.setBindingRegCount, bindState.boundSetCount);

        if (count > 0)
        {
            utils::IterateMask deviceGroup(m_curDeviceMask);
            do
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdSetUserData(
                    palBindPoint,
                    userDataLayout.setBindingRegBase,
                    count,
                    PerGpuState(deviceIdx)->setBindingData[apiBindPoint]);
            }
            while (deviceGroup.IterateNext());
        }
    }

    if ((flags & RebindUserDataPushConstants) != 0)
    {
        const uint32_t count = Util::Min(userDataLayout.pushConstRegCount, bindState.pushedConstCount);

        if (count > 0)
        {
            // perDeviceStride is zero here because push constant data is replicated for all devices.
            // Note: There might be interesting use cases where don't want to clone this data.
            const uint32_t perDeviceStride = 0;

            PalCmdBufferSetUserData(
                palBindPoint,
                userDataLayout.pushConstRegBase,
                count,
                perDeviceStride,
                bindState.pushConstData);
        }
    }

    if (((flags & RebindUberFetchInternalMem) != 0) && (bindState.pVertexInputInternalData != nullptr))
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdSetUserData(
                palBindPoint,
                userDataLayout.uberFetchConstBufRegBase,
                2,
                reinterpret_cast<const uint32_t*>(&bindState.pVertexInputInternalData->gpuAddress[deviceIdx]));
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
// Insert secondary command buffers into a primary command buffer
void CmdBuffer::ExecuteCommands(
    uint32_t                                    cmdBufferCount,
    const VkCommandBuffer*                      pCmdBuffers)
{
    DbgBarrierPreCmd(DbgBarrierExecuteCommands);

    for (uint32_t i = 0; i < cmdBufferCount; i++)
    {
        CmdBuffer* pInteralCmdBuf = ApiCmdBuffer::ObjectFromHandle(pCmdBuffers[i]);

#if VKI_RAY_TRACING
        m_flags.hasRayTracing |= pInteralCmdBuf->HasRayTracing();
#endif

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::ICmdBuffer* pPalNestedCmdBuffer = pInteralCmdBuf->PalCmdBuffer(deviceIdx);
            PalCmdBuffer(deviceIdx)->CmdExecuteNestedCmdBuffers(1, &pPalNestedCmdBuffer);
        }
        while (deviceGroup.IterateNext());
    }

    // Executing secondary command buffer will clear the states of Graphic Pipeline
    // in that case they cannot be used after ends of execution secondary command buffer
    ResetPipelineState();

    DbgBarrierPostCmd(DbgBarrierExecuteCommands);
}

// =====================================================================================================================
// Destroy a command buffer object
VkResult CmdBuffer::Destroy(void)
{
    Instance* const pInstance = m_pDevice->VkInstance();

    for (uint32 i = 0; i < PipelineBindCount; ++i)
    {
        pInstance->FreeMem(m_allGpuState.pipelineState[i].pPushDescriptorSetMemory);
    }

    if (m_pSqttState != nullptr)
    {
        Util::Destructor(m_pSqttState);

        pInstance->FreeMem(m_pSqttState);
    }

    if (m_pTransformFeedbackState != nullptr)
    {
        pInstance->FreeMem(m_pTransformFeedbackState);
    }

    if (m_pUberFetchShaderTempBuffer != nullptr)
    {
        pInstance->FreeMem(m_pUberFetchShaderTempBuffer);
    }

    // Unregister this command buffer from the pool
    m_pCmdPool->UnregisterCmdBuffer(this);

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); ++deviceIdx)
    {
        if (m_pBackupPalCmdBuffers[deviceIdx] != nullptr)
        {
            m_pBackupPalCmdBuffers[deviceIdx]->Destroy();
            m_pCmdPool->GetCmdPoolAllocator()->pfnFree(
                m_pCmdPool->GetCmdPoolAllocator()->pUserData,
                m_pBackupPalCmdBuffers[deviceIdx]);
        }
    }

    PalCmdBufferDestroy();

    ReleaseResources();

#if VKI_RAY_TRACING
    FreeRayTracingIndirectMemory();

#endif

    m_debugPrintf.Reset(m_pDevice);
    Util::Destructor(this);

    m_pDevice->FreeApiObject(m_pCmdPool->GetCmdPoolAllocator(), ApiCmdBuffer::FromObject(this));

    return VK_SUCCESS;
}

// =====================================================================================================================
void CmdBuffer::ReleaseResources()
{
    auto              pInstance = m_pDevice->VkInstance();
    RenderStateCache* pRSCache  = m_pDevice->GetRenderStateCache();

    for (uint32_t i = 0; i < m_palDepthStencilState.NumElements(); ++i)
    {
        pRSCache->DestroyDepthStencilState(
            m_palDepthStencilState.At(i).pPalDepthStencil, pInstance->GetAllocCallbacks());
    }

    m_palDepthStencilState.Clear();

    for (uint32_t i = 0; i < m_palColorBlendState.NumElements(); ++i)
    {
        pRSCache->DestroyColorBlendState(m_palColorBlendState.At(i).pPalColorBlend, pInstance->GetAllocCallbacks());
    }

    m_palColorBlendState.Clear();

    for (uint32_t i = 0; i < m_palMsaaState.NumElements(); ++i)
    {
        pRSCache->DestroyMsaaState(m_palMsaaState.At(i).pPalMsaa, pInstance->GetAllocCallbacks());
    }

    m_palMsaaState.Clear();

    // Release per-attachment render pass instance memory
    if (m_renderPassInstance.pAttachments != nullptr)
    {
        pInstance->FreeMem(m_renderPassInstance.pAttachments);

        m_renderPassInstance.pAttachments       = nullptr;
        m_renderPassInstance.maxAttachmentCount = 0;
    }

    // Release per-subpass instance memory
    if (m_renderPassInstance.pSamplePatterns != nullptr)
    {
        pInstance->FreeMem(m_renderPassInstance.pSamplePatterns);

        m_renderPassInstance.pSamplePatterns = nullptr;
        m_renderPassInstance.maxSubpassCount = 0;
    }

    if (m_pStackAllocator != nullptr)
    {
        pInstance->StackMgr()->ReleaseAllocator(m_pStackAllocator);

        m_pStackAllocator = nullptr;
    }
}

// =====================================================================================================================
template <uint32_t numPalDevices, bool useCompactDescriptor>
void CmdBuffer::BindDescriptorSets(
    VkPipelineBindPoint    pipelineBindPoint,
    VkPipelineLayout       layout,
    uint32_t               firstSet,
    uint32_t               setCount,
    const VkDescriptorSet* pDescriptorSets,
    uint32_t               dynamicOffsetCount,
    const uint32_t*        pDynamicOffsets)
{
    DbgBarrierPreCmd(DbgBarrierBindSetsPushConstants);

    if (setCount > 0)
    {
        Pal::PipelineBindPoint palBindPoint;
        PipelineBindPoint      apiBindPoint;

        ConvertPipelineBindPoint(pipelineBindPoint, &palBindPoint, &apiBindPoint);

        const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(layout);

        // Get user data register information from the given pipeline layout
        const PipelineLayout::Info& layoutInfo = pLayout->GetInfo();

        // Update descriptor set binding data shadow.
        VK_ASSERT((firstSet + setCount) <= layoutInfo.setCount);

        for (uint32_t i = 0; i < setCount; ++i)
        {
            if (pDescriptorSets[i] != VK_NULL_HANDLE)
            {
                // Compute set binding point index
                const uint32_t setBindIdx = firstSet + i;

                // User data information for this set
                const PipelineLayout::SetUserDataLayout& setLayoutInfo = pLayout->GetSetUserData(setBindIdx);

                // If this descriptor set has any dynamic descriptor data then write them into the shadow.
                if (setLayoutInfo.dynDescCount > 0)
                {
                    // NOTE: We supply patched SRDs directly in used data registers.
                    utils::IterateMask deviceGroup(m_curDeviceMask);
                    do
                    {
                        const uint32_t deviceIdx = deviceGroup.Index();

                        DescriptorSet<numPalDevices>::PatchedDynamicDataFromHandle(
                            pDescriptorSets[i],
                            deviceIdx,
                            &(PerGpuState(deviceIdx)->
                                setBindingData[apiBindPoint][setLayoutInfo.dynDescDataRegOffset]),
                            pDynamicOffsets,
                            setLayoutInfo.dynDescCount,
                            useCompactDescriptor);

                    }
                    while (deviceGroup.IterateNext());

                    // Skip over the already consumed dynamic offsets.
                    pDynamicOffsets += setLayoutInfo.dynDescCount;
                }

                // If this descriptor set needs a set pointer, then write it to the shadow.
                if (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg)
                {
                    utils::IterateMask deviceGroup(m_curDeviceMask);

                    do
                    {
                        const uint32_t deviceIdx = deviceGroup.Index();

                        DescriptorSet<numPalDevices>::UserDataPtrValueFromHandle(
                            pDescriptorSets[i],
                            deviceIdx,
                            &(PerGpuState(deviceIdx)->
                                setBindingData[apiBindPoint][setLayoutInfo.setPtrRegOffset]));
                    }
                    while (deviceGroup.IterateNext());
                }
            }
        }

        SetUserDataPipelineLayout(firstSet, setCount, pLayout, palBindPoint, apiBindPoint);
    }

    DbgBarrierPostCmd(DbgBarrierBindSetsPushConstants);
}

// =====================================================================================================================
void CmdBuffer::BindDescriptorSetsBuffers(
    VkPipelineBindPoint      pipelineBindPoint,
    VkPipelineLayout         layout,
    uint32_t                 firstSet,
    uint32_t                 setCount,
    const DescriptorBuffers* pDescriptorBuffers)
{
    DbgBarrierPreCmd(DbgBarrierBindSetsPushConstants);

    if (setCount > 0)
    {
        Pal::PipelineBindPoint palBindPoint;
        PipelineBindPoint      apiBindPoint;

        ConvertPipelineBindPoint(pipelineBindPoint, &palBindPoint, &apiBindPoint);

        const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(layout);

        // Get user data register information from the given pipeline layout
        const PipelineLayout::Info& layoutInfo = pLayout->GetInfo();

        // Update descriptor set binding data shadow.
        VK_ASSERT((firstSet + setCount) <= layoutInfo.setCount);

        for (uint32_t i = 0; i < setCount; ++i)
        {
            // Compute set binding point index
            const uint32_t setBindIdx = firstSet + i;

            // User data information for this set
            const PipelineLayout::SetUserDataLayout& setLayoutInfo = pLayout->GetSetUserData(setBindIdx);

            // If this descriptor set needs a set pointer, then write it to the shadow.
            if (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg)
            {
                utils::IterateMask deviceGroup(m_curDeviceMask);

                do
                {
                    const uint32_t deviceIdx = deviceGroup.Index();

                    const DescBufBinding& bufBinding = *m_allGpuState.pDescBufBinding;
                    PerGpuRenderState* pPerGpuState = PerGpuState(deviceIdx);

                    Pal::gpusize bufferAddress = bufBinding.baseAddr[pDescriptorBuffers[setBindIdx].baseAddrNdx];
                    Pal::gpusize offset = pDescriptorBuffers[setBindIdx].offset;

                    pPerGpuState->setBindingData[apiBindPoint][setLayoutInfo.setPtrRegOffset] =
                        static_cast<uint32_t>((bufferAddress + offset) & 0xFFFFFFFFull);
                }
                while (deviceGroup.IterateNext());
            }
        }

        SetUserDataPipelineLayout(firstSet, setCount, pLayout, palBindPoint, apiBindPoint);
    }

    DbgBarrierPostCmd(DbgBarrierBindSetsPushConstants);
}

// =====================================================================================================================
void CmdBuffer::SetUserDataPipelineLayout(
    uint32_t                      firstSet,
    uint32_t                      setCount,
    const PipelineLayout*         pLayout,
    const Pal::PipelineBindPoint  palBindPoint,
    const PipelineBindPoint       apiBindPoint)
{
    VK_ASSERT(setCount > 0);

    // Get user data register information from the given pipeline layout
    const PipelineLayout::Info& layoutInfo = pLayout->GetInfo();

    if (pLayout->GetScheme() == PipelineLayoutScheme::Compact)
    {
        // Get the current binding state in the command buffer
        PipelineBindState* pBindState = &m_allGpuState.pipelineState[apiBindPoint];

        // Figure out the total range of user data registers written by this sequence of descriptor set binds
        const PipelineLayout::SetUserDataLayout& firstSetLayout = pLayout->GetSetUserData(firstSet);
        const PipelineLayout::SetUserDataLayout& lastSetLayout = pLayout->GetSetUserData(firstSet + setCount - 1);

        const uint32_t rangeOffsetBegin = firstSetLayout.firstRegOffset;
        const uint32_t rangeOffsetEnd = lastSetLayout.firstRegOffset + lastSetLayout.totalRegCount;

        // Update the high watermark of number of user data entries written for currently bound descriptor sets and
        // their dynamic offsets in the current command buffer state.
        pBindState->boundSetCount = Util::Max(pBindState->boundSetCount, rangeOffsetEnd);

        // Descriptor set with zero resource binding is allowed in spec, so we need to check this and only proceed
        // when there are at least 1 user data to update.
        const uint32_t rangeRegCount = rangeOffsetEnd - rangeOffsetBegin;

        if (rangeRegCount > 0)
        {
            // Program the user data register only if the current user data layout base matches that of the given
            // layout.  Otherwise, what's happening is that the application is binding descriptor sets for a future
            // pipeline layout (e.g. at the top of the command buffer) and this register write will be redundant.
            // A future vkCmdBindPipeline will reprogram the user data register.
            if (PalPipelineBindingOwnedBy(palBindPoint, apiBindPoint) &&
                (pBindState->userDataLayout.compact.setBindingRegBase ==
                    layoutInfo.userDataLayout.compact.setBindingRegBase))
            {
                utils::IterateMask deviceGroup(m_curDeviceMask);
                do
                {
                    const uint32_t deviceIdx = deviceGroup.Index();

                    PalCmdBuffer(deviceIdx)->CmdSetUserData(
                        palBindPoint,
                        pBindState->userDataLayout.compact.setBindingRegBase + rangeOffsetBegin,
                        rangeRegCount,
                        &(PerGpuState(deviceIdx)->setBindingData[apiBindPoint][rangeOffsetBegin]));
                }
                while (deviceGroup.IterateNext());
            }
        }
    }
    else if (pLayout->GetScheme() == PipelineLayoutScheme::Indirect)
    {
        const auto& userDataLayout = layoutInfo.userDataLayout.indirect;

        for (uint32_t setIdx = firstSet; setIdx < firstSet + setCount; ++setIdx)
        {
            const PipelineLayout::SetUserDataLayout& setLayoutInfo = pLayout->GetSetUserData(setIdx);

            utils::IterateMask deviceGroup(m_curDeviceMask);
            do
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                if (setLayoutInfo.dynDescCount > 0)
                {
                    const uint32_t dynBufferSizeDw =
                        setLayoutInfo.dynDescCount * DescriptorSetLayout::GetDynamicBufferDescDwSize(m_pDevice);

                    Pal::gpusize gpuAddr;

                    void* pCpuAddr = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(
                        dynBufferSizeDw,
                        m_pDevice->GetProperties().descriptorSizes.alignmentInDwords,
                        &gpuAddr);

                    const uint32_t gpuAddrLow = static_cast<uint32_t>(gpuAddr);

                    memcpy(pCpuAddr,
                        &(PerGpuState(deviceIdx)->setBindingData[apiBindPoint][setLayoutInfo.dynDescDataRegOffset]),
                        dynBufferSizeDw * sizeof(uint32_t));

                    PalCmdBuffer(deviceIdx)->CmdSetUserData(
                        palBindPoint,
                        userDataLayout.setBindingPtrRegBase + 2 * setIdx * PipelineLayout::SetPtrRegCount,
                        PipelineLayout::SetPtrRegCount,
                        &gpuAddrLow);
                }

                if (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg)
                {
                    PalCmdBuffer(deviceIdx)->CmdSetUserData(
                        palBindPoint,
                        userDataLayout.setBindingPtrRegBase + (2 * setIdx + 1) * PipelineLayout::SetPtrRegCount,
                        PipelineLayout::SetPtrRegCount,
                        &(PerGpuState(deviceIdx)->setBindingData[apiBindPoint][setLayoutInfo.setPtrRegOffset]));
                }
            }
            while (deviceGroup.IterateNext());
        }
    }
    else
    {
        VK_NEVER_CALLED();
    }
}

// =====================================================================================================================
template<uint32_t numPalDevices, bool useCompactDescriptor>
VKAPI_ATTR void VKAPI_CALL CmdBuffer::CmdBindDescriptorSets(
    VkCommandBuffer                             cmdBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    firstSet,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets,
    uint32_t                                    dynamicOffsetCount,
    const uint32_t*                             pDynamicOffsets)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BindDescriptorSets<numPalDevices, useCompactDescriptor>(
        pipelineBindPoint,
        layout,
        firstSet,
        descriptorSetCount,
        pDescriptorSets,
        dynamicOffsetCount,
        pDynamicOffsets);
}

// =====================================================================================================================
PFN_vkCmdBindDescriptorSets CmdBuffer::GetCmdBindDescriptorSetsFunc(
    const Device* pDevice)
{
    PFN_vkCmdBindDescriptorSets pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
        case 1:
            pFunc = GetCmdBindDescriptorSetsFunc<1>(pDevice);
            break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        case 2:
            pFunc = GetCmdBindDescriptorSetsFunc<2>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
        case 3:
            pFunc = GetCmdBindDescriptorSetsFunc<3>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
        case 4:
            pFunc = GetCmdBindDescriptorSetsFunc<4>(pDevice);
            break;
#endif
        default:
            pFunc = nullptr;
            VK_NEVER_CALLED();
            break;
    }

    return pFunc;
}

// =====================================================================================================================
template <uint32_t numPalDevices>
PFN_vkCmdBindDescriptorSets CmdBuffer::GetCmdBindDescriptorSetsFunc(
    const Device* pDevice)
{
    PFN_vkCmdBindDescriptorSets pFunc = nullptr;

    if (pDevice->UseCompactDynamicDescriptors())
    {
        pFunc = CmdBindDescriptorSets<numPalDevices, true>;
    }
    else
    {
        pFunc = CmdBindDescriptorSets<numPalDevices, false>;
    }

    return pFunc;
}

// =====================================================================================================================
template <size_t imageDescSize,
          size_t samplerDescSize,
          size_t bufferDescSize,
          uint32_t numPalDevices>
VKAPI_ATTR void VKAPI_CALL CmdBuffer::CmdPushDescriptorSetKHR(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);

    pCmdBuffer->PushDescriptorSetKHR<imageDescSize, samplerDescSize, bufferDescSize, numPalDevices>(
        pipelineBindPoint,
        layout,
        set,
        descriptorWriteCount,
        pDescriptorWrites);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
PFN_vkCmdPushDescriptorSetKHR CmdBuffer::GetCmdPushDescriptorSetKHRFunc(
    const Device* pDevice)
{
    const size_t imageDescSize   = pDevice->GetProperties().descriptorSizes.imageView;
    const size_t samplerDescSize = pDevice->GetProperties().descriptorSizes.sampler;
    const size_t bufferDescSize  = pDevice->GetProperties().descriptorSizes.bufferView;

    PFN_vkCmdPushDescriptorSetKHR pFunc = nullptr;

    if ((imageDescSize   == 32) &&
        (samplerDescSize == 16) &&
        (bufferDescSize  == 16))
    {
        pFunc = &CmdPushDescriptorSetKHR<
            32,
            16,
            16,
            numPalDevices>;
    }
    else
    {
        VK_NEVER_CALLED();
    }

    return pFunc;
}

// =====================================================================================================================
PFN_vkCmdPushDescriptorSetKHR CmdBuffer::GetCmdPushDescriptorSetKHRFunc(
    const Device* pDevice)
{
    PFN_vkCmdPushDescriptorSetKHR pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
        case 1:
            pFunc = GetCmdPushDescriptorSetKHRFunc<1>(pDevice);
            break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        case 2:
            pFunc = GetCmdPushDescriptorSetKHRFunc<2>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
        case 3:
            pFunc = GetCmdPushDescriptorSetKHRFunc<3>(pDevice);
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
        case 4:
            pFunc = GetCmdPushDescriptorSetKHRFunc<4>(pDevice);
            break;
#endif
        default:
            VK_NEVER_CALLED();
            break;
    }

    return pFunc;
}

// =====================================================================================================================
template <uint32_t numPalDevices>
VKAPI_ATTR void VKAPI_CALL CmdBuffer::CmdPushDescriptorSetWithTemplateKHR(
    VkCommandBuffer                             commandBuffer,
    VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    const void*                                 pData)
{
    CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(commandBuffer);

    pCmdBuffer->PushDescriptorSetWithTemplateKHR<numPalDevices>(
        descriptorUpdateTemplate,
        layout,
        set,
        pData);
}

// =====================================================================================================================
PFN_vkCmdPushDescriptorSetWithTemplateKHR CmdBuffer::GetCmdPushDescriptorSetWithTemplateKHRFunc(
    const Device* pDevice)
{
    PFN_vkCmdPushDescriptorSetWithTemplateKHR pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
        case 1:
            pFunc = CmdPushDescriptorSetWithTemplateKHR<1>;
            break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
        case 2:
            pFunc = CmdPushDescriptorSetWithTemplateKHR<2>;
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
        case 3:
            pFunc = CmdPushDescriptorSetWithTemplateKHR<3>;
            break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
        case 4:
            pFunc = CmdPushDescriptorSetWithTemplateKHR<4>;
            break;
#endif
        default:
            VK_NEVER_CALLED();
            break;
    }

    return pFunc;
}

// =====================================================================================================================
void CmdBuffer::BindIndexBuffer(
    VkBuffer     buffer,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkIndexType  indexType)
{
    DbgBarrierPreCmd(DbgBarrierBindIndexVertexBuffer);

    const Pal::IndexType palIndexType = VkToPalIndexType(indexType);
    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    if (pBuffer != NULL)
    {
        PalCmdBindIndexData(pBuffer, offset, palIndexType, size);
    }
    else
    {
        PalCmdUnbindIndexData(palIndexType);
    }

    DbgBarrierPostCmd(DbgBarrierBindIndexVertexBuffer);
}

// =====================================================================================================================
// Initializes VB binding manager state.  Should be called when the command buffer is being initialized.
void CmdBuffer::InitializeVertexBuffer()
{
    for (uint32 deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        Pal::BufferViewInfo* pBindings = &PerGpuState(deviceIdx)->vbBindings[0];

        for (uint32 i = 0; i < Pal::MaxVertexBuffers; ++i)
        {
            // Format needs to be set to invalid for struct srv SRDs
            pBindings[i].swizzledFormat  = Pal::UndefinedSwizzledFormat;
            pBindings[i].gpuAddr         = 0;
            pBindings[i].range           = 0;
            pBindings[i].stride          = 0;
            pBindings[i].flags.u32All    = 0;
        }
    }

    m_vbWatermark = 0;
}

// =====================================================================================================================
// Called to reset the state of the VB manager because the parent command buffer is being reset.
void CmdBuffer::ResetVertexBuffer()
{
    for (uint32 deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        Pal::BufferViewInfo* pBindings = &PerGpuState(deviceIdx)->vbBindings[0];

        for (uint32 i = 0; i < m_vbWatermark; ++i)
        {
            pBindings[i].gpuAddr = 0;
            pBindings[i].range   = 0;
            pBindings[i].stride  = 0;
        }
    }

    m_vbWatermark = 0;

    m_uberFetchShaderInternalDataMap.Reset();
}

// =====================================================================================================================
// Implementation of vkCmdBindVertexBuffers
void CmdBuffer::BindVertexBuffers(
    uint32_t            firstBinding,
    uint32_t            bindingCount,
    const VkBuffer*     pBuffers,
    const VkDeviceSize* pOffsets,
    const VkDeviceSize* pSizes,
    const VkDeviceSize* pStrides)
{
    if (bindingCount > 0)
    {
        DbgBarrierPreCmd(DbgBarrierBindIndexVertexBuffer);

        const bool padVertexBuffers = m_flags.padVertexBuffers;

        utils::IterateMask deviceGroup(GetDeviceMask());
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::BufferViewInfo* pBinding = &PerGpuState(deviceIdx)->vbBindings[firstBinding];
            Pal::BufferViewInfo* pEndBinding = pBinding + bindingCount;
            uint32_t             inputIdx = 0;

            while (pBinding != pEndBinding)
            {
                const VkBuffer     buffer = pBuffers[inputIdx];
                const VkDeviceSize offset = pOffsets[inputIdx];

                if (buffer != VK_NULL_HANDLE)
                {
                    const Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

                    pBinding->gpuAddr = pBuffer->GpuVirtAddr(deviceIdx) + offset;
                    if ((pSizes != nullptr) && (pSizes[inputIdx] != VK_WHOLE_SIZE))
                    {
                        pBinding->range = pSizes[inputIdx];
                    }
                    else
                    {
                        pBinding->range = pBuffer->GetSize() - offset;
                    }
                }
                else
                {
                    pBinding->gpuAddr = 0;
                    pBinding->range = 0;
                }

                if (pStrides != nullptr)
                {
                    pBinding->stride = pStrides[inputIdx];
                }

                if (padVertexBuffers && (pBinding->stride != 0))
                {
                    pBinding->range = Util::RoundUpToMultiple(pBinding->range, pBinding->stride);
                }

                inputIdx++;
                pBinding++;
            }

            PalCmdBuffer(deviceIdx)->CmdSetVertexBuffers(
                firstBinding, bindingCount, &PerGpuState(deviceIdx)->vbBindings[firstBinding]);
        }
        while (deviceGroup.IterateNext());

        m_vbWatermark = Util::Max(m_vbWatermark, firstBinding + bindingCount);

        DbgBarrierPostCmd(DbgBarrierBindIndexVertexBuffer);
    }
}

// =====================================================================================================================
void CmdBuffer::UpdateVertexBufferStrides(
    const GraphicsPipeline* pPipeline)
{
    VK_ASSERT(pPipeline != nullptr);

    // Update strides for each binding used by the graphics pipeline.  Rebuild SRD data for those bindings
    // whose strides changed.

    const bool padVertexBuffers = m_flags.padVertexBuffers;

    utils::IterateMask deviceGroup(GetDeviceMask());
    do
    {
        const VbBindingInfo& bindingInfo = pPipeline->GetVbBindingInfo();

        uint32 deviceIdx = deviceGroup.Index();

        uint32 firstChanged = UINT_MAX;
        uint32 lastChanged  = 0;
        uint32 count        = bindingInfo.bindingCount;

        Pal::BufferViewInfo* pVbBindings = PerGpuState(deviceIdx)->vbBindings;

        for (uint32 bindex = 0; bindex < count; ++bindex)
        {
            uint32 slot                   = bindingInfo.bindings[bindex].slot;
            uint32 byteStride             = bindingInfo.bindings[bindex].byteStride;
            Pal::BufferViewInfo* pBinding = &pVbBindings[slot];

            if (pBinding->stride != byteStride)
            {
                pBinding->stride = byteStride;

                if (pBinding->gpuAddr != 0)
                {
                    firstChanged = Util::Min(firstChanged, slot);
                    lastChanged  = Util::Max(lastChanged, slot);
                }

                if (padVertexBuffers && (pBinding->stride != 0))
                {
                    pBinding->range = Util::RoundUpToMultiple(pBinding->range, pBinding->stride);
                }
            }
        }

        if (firstChanged <= lastChanged)
        {
            PalCmdBuffer(deviceIdx)->CmdSetVertexBuffers(
                firstChanged, (lastChanged - firstChanged) + 1,
                &PerGpuState(deviceIdx)->vbBindings[firstChanged]);
        }
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::Draw(
    uint32_t firstVertex,
    uint32_t vertexCount,
    uint32_t firstInstance,
    uint32_t instanceCount)
{
    DbgBarrierPreCmd(DbgBarrierDrawNonIndexed);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    {
        PalCmdDraw(firstVertex,
            vertexCount,
            firstInstance,
            instanceCount,
            0u);
    }

    DbgBarrierPostCmd(DbgBarrierDrawNonIndexed);
}

// =====================================================================================================================
void CmdBuffer::DrawIndexed(
    uint32_t firstIndex,
    uint32_t indexCount,
    int32_t  vertexOffset,
    uint32_t firstInstance,
    uint32_t instanceCount)
{
    DbgBarrierPreCmd(DbgBarrierDrawIndexed);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    {
        PalCmdDrawIndexed(firstIndex,
                          indexCount,
                          vertexOffset,
                          firstInstance,
                          instanceCount,
                          0u);
    }

    DbgBarrierPostCmd(DbgBarrierDrawIndexed);
}

// =====================================================================================================================
template< bool indexed, bool useBufferCount>
void CmdBuffer::DrawIndirect(
    VkBuffer     buffer,
    VkDeviceSize offset,
    uint32_t     count,
    uint32_t     stride,
    VkBuffer     countBuffer,
    VkDeviceSize countOffset)
{
    DbgBarrierPreCmd((indexed ? DbgBarrierDrawIndexed : DbgBarrierDrawNonIndexed) | DbgBarrierDrawIndirect);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    if ((stride + offset) <= pBuffer->PalMemory(DefaultDeviceIndex)->Desc().size)
    {
        const Pal::gpusize paramOffset = pBuffer->MemOffset() + offset;
        Pal::gpusize countVirtAddr = 0;

        utils::IterateMask deviceGroup(m_curDeviceMask);

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            if (useBufferCount)
            {
                Buffer* pCountBuffer = Buffer::ObjectFromHandle(countBuffer);
                countVirtAddr = pCountBuffer->GpuVirtAddr(deviceIdx) + countOffset;
            }

            if (indexed == false)
            {
                PalCmdBuffer(deviceIdx)->CmdDrawIndirectMulti(
                    *pBuffer->PalMemory(deviceIdx),
                    paramOffset,
                    stride,
                    count,
                    countVirtAddr);
            }
            else
            {
                PalCmdBuffer(deviceIdx)->CmdDrawIndexedIndirectMulti(
                    *pBuffer->PalMemory(deviceIdx),
                    paramOffset,
                    stride,
                    count,
                    countVirtAddr);
            }
        }
        while (deviceGroup.IterateNext());
    }

    DbgBarrierPostCmd((indexed ? DbgBarrierDrawIndexed : DbgBarrierDrawNonIndexed) | DbgBarrierDrawIndirect);
}

// =====================================================================================================================
void CmdBuffer::DrawMeshTasks(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    DbgBarrierPreCmd(DbgBarrierDrawMeshTasks);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    PalCmdDrawMeshTasks(x, y, z);

    DbgBarrierPostCmd(DbgBarrierDrawMeshTasks);
}

// =====================================================================================================================
template<bool useBufferCount>
void CmdBuffer::DrawMeshTasksIndirect(
    VkBuffer     buffer,
    VkDeviceSize offset,
    uint32_t     count,
    uint32_t     stride,
    VkBuffer     countBuffer,
    VkDeviceSize countOffset)
{
    DbgBarrierPreCmd(DbgBarrierDrawMeshTasksIndirect);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    PalCmdDrawMeshTasksIndirect<useBufferCount>(buffer, offset, count, stride, countBuffer, countOffset);

    DbgBarrierPostCmd(DbgBarrierDrawMeshTasksIndirect);
}

// =====================================================================================================================
void CmdBuffer::Dispatch(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    DbgBarrierPreCmd(DbgBarrierDispatch);

    if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindCompute) == false)
    {
        RebindPipeline<PipelineBindCompute, false>();
    }

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pComputePipeline, Pal::PipelineBindPoint::Compute, x, y, z, nullptr, 0);
#endif

    if (m_pDevice->GetRuntimeSettings().enableAlternatingThreadGroupOrder)
    {
        BindAlternatingThreadGroupConstant();
    }

    PalCmdDispatch(x, y, z);

    DbgBarrierPostCmd(DbgBarrierDispatch);
}

// =====================================================================================================================
void CmdBuffer::DispatchOffset(
    uint32_t                    base_x,
    uint32_t                    base_y,
    uint32_t                    base_z,
    uint32_t                    dim_x,
    uint32_t                    dim_y,
    uint32_t                    dim_z)
{
    DbgBarrierPreCmd(DbgBarrierDispatch);

    if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindCompute) == false)
    {
        RebindPipeline<PipelineBindCompute, false>();
    }

#if VKI_RAY_TRACING
    BindRayQueryConstants(
        m_allGpuState.pComputePipeline, Pal::PipelineBindPoint::Compute, dim_x, dim_y, dim_z, nullptr, 0);
#endif

    PalCmdDispatchOffset(base_x, base_y, base_z, dim_x, dim_y, dim_z);

    DbgBarrierPostCmd(DbgBarrierDispatch);
}

// =====================================================================================================================
void CmdBuffer::DispatchIndirect(
    VkBuffer        buffer,
    VkDeviceSize    offset)
{
    DbgBarrierPreCmd(DbgBarrierDispatchIndirect);

    if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindCompute) == false)
    {
        RebindPipeline<PipelineBindCompute, false>();
    }

    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pComputePipeline,
                          Pal::PipelineBindPoint::Compute,
                          0,
                          0,
                          0,
                          pBuffer,
                          offset);
#endif

    PalCmdDispatchIndirect(pBuffer, offset);

    DbgBarrierPostCmd(DbgBarrierDispatchIndirect);
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
        palCopyInfo.flags.dstAsSrgb = pDstImage->TreatAsSrgb();

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
        fillSize = Util::RoundDownToMultiple(pDestBuffer->GetSize() - destOffset, static_cast<VkDeviceSize>(sizeof(data) ) );
    }

    PalCmdFillBuffer(pDestBuffer, pDestBuffer->MemOffset() + destOffset, fillSize, data);

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierCopyBuffer);
}

// =====================================================================================================================
// Performs a color clear (vkCmdClearColorImage)
void CmdBuffer::ClearColorImage(
    VkImage                        image,
    VkImageLayout                  imageLayout,
    const VkClearColorValue*       pColor,
    uint32_t                       rangeCount,
    const VkImageSubresourceRange* pRanges)
{
    PalCmdSuspendPredication(true);

    const Image* pImage = Image::ObjectFromHandle(image);

    VkFormat format = pImage->TreatAsSrgb() ? pImage->GetSrgbFormat() : pImage->GetFormat();

    const Pal::SwizzledFormat palFormat = VkToPalFormat(format, m_pDevice->GetRuntimeSettings());

    if (Pal::Formats::IsBlockCompressed(palFormat.format))
    {
        return;
    }

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRanges  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRanges)), MaxPalColorAspectsPerMask);
    auto       rangeBatch = Util::Min(rangeCount * MaxPalColorAspectsPerMask, maxRanges);

    // Allocate space to store image subresource ranges
    Pal::SubresRange* pPalRanges = virtStackFrame.AllocArray<Pal::SubresRange>(rangeBatch);

    if (pPalRanges != nullptr)
    {
        const Pal::ImageLayout layout = pImage->GetBarrierPolicy().GetTransferLayout(
            imageLayout, GetQueueFamilyIndex());

        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount;)
        {
            uint32_t palRangeCount = 0;

            while ((rangeIdx < rangeCount) &&
                   (palRangeCount <= (rangeBatch - MaxPalColorAspectsPerMask)))
            {
                // Only color aspect is allowed here
                VK_ASSERT(pRanges[rangeIdx].aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);

                VkToPalSubresRange(pImage->GetFormat(),
                                   pRanges[rangeIdx],
                                   pImage->GetMipLevels(),
                                   pImage->GetArraySize(),
                                   pPalRanges,
                                   &palRangeCount,
                                   m_pDevice->GetRuntimeSettings());

                ++rangeIdx;
            }

            PalCmdClearColorImage(
                *pImage,
                layout,
                VkToPalClearColor(*pColor, palFormat),
                palFormat,
                palRangeCount,
                pPalRanges,
                0,
                nullptr,
                0);
        }

        virtStackFrame.FreeArray(pPalRanges);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);
}

// =====================================================================================================================
bool CmdBuffer::PreBltBindMsaaState(
    const Image& image)
{
    const Pal::IMsaaState* const* pBltMsaa = nullptr;

    if (GetPalQueueType() == Pal::QueueTypeUniversal)
    {
        const Pal::ImageCreateInfo& imgInfo = image.PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

        if (imgInfo.samples > 1)
        {
            pBltMsaa = m_pDevice->GetBltMsaaState(imgInfo.samples);
        }

        PalCmdBindMsaaStates(pBltMsaa);
    }

    return (pBltMsaa != nullptr) ? true : false;
}

// =====================================================================================================================
void CmdBuffer::PostBltRestoreMsaaState(
    bool bltMsaaState)
{
    if (GetPalQueueType() == Pal::QueueTypeUniversal)
    {
        if (bltMsaaState &&
            (m_allGpuState.pGraphicsPipeline != nullptr))
        {
            if (m_allGpuState.pGraphicsPipeline->GetPipelineFlags().bindMsaaObject)
            {
                PalCmdBindMsaaStates(m_allGpuState.pGraphicsPipeline->GetMsaaStates());
            }
            else
            {
                m_allGpuState.dirtyGraphics.msaa = 1;
            }
        }
    }
}

// =====================================================================================================================
// Performs a depth-stencil clear of an image (vkCmdClearDepthStencilImage)
void CmdBuffer::ClearDepthStencilImage(
    VkImage                        image,
    VkImageLayout                  imageLayout,
    float                          depth,
    uint32_t                       stencil,
    uint32_t                       rangeCount,
    const VkImageSubresourceRange* pRanges)
{
    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRanges  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRanges)), MaxPalDepthAspectsPerMask);
    auto       rangeBatch = Util::Min(rangeCount * MaxPalDepthAspectsPerMask, maxRanges);

    // Allocate space to store image subresource ranges (we need a separate region per PAL aspect)
    Pal::SubresRange* pPalRanges = virtStackFrame.AllocArray<Pal::SubresRange>(rangeBatch);

    if (pPalRanges != nullptr)
    {
        const Image* pImage           = Image::ObjectFromHandle(image);
        const Pal::ImageLayout layout = pImage->GetBarrierPolicy().GetTransferLayout(
            imageLayout, GetQueueFamilyIndex());

        ValidateSamplePattern(pImage->GetImageSamples(), nullptr);

        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount;)
        {
            uint32_t palRangeCount = 0;

            while ((rangeIdx < rangeCount) &&
                   (palRangeCount <= (rangeBatch - MaxPalDepthAspectsPerMask)))
            {
                // Only depth or stencil aspect is allowed here
                VK_ASSERT((pRanges[rangeIdx].aspectMask & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0);

                VkToPalSubresRange(pImage->GetFormat(),
                                   pRanges[rangeIdx],
                                   pImage->GetMipLevels(),
                                   pImage->GetArraySize(),
                                   pPalRanges,
                                   &palRangeCount,
                                   m_pDevice->GetRuntimeSettings());

                ++rangeIdx;
            }

            PalCmdClearDepthStencil(
                *pImage,
                layout,
                layout,
                VkToPalClearDepth(depth),
                stencil,
                palRangeCount,
                pPalRanges,
                0,
                nullptr,
                0);
        }

        virtStackFrame.FreeArray(pPalRanges);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);
}

// =====================================================================================================================
// Clears a set of attachments in the current subpass
void CmdBuffer::ClearAttachments(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    // if pRenderPass is null, than dynamic rendering is being used
    if (m_allGpuState.pRenderPass == nullptr)
    {
        if (m_flags.is2ndLvl == false)
        {
            ClearDynamicRenderingImages(attachmentCount, pAttachments, rectCount, pRects);
        }
        else
        {
            ClearDynamicRenderingBoundAttachments(attachmentCount, pAttachments, rectCount, pRects);
        }
    }
    else
    {
        if ((m_flags.is2ndLvl == false) && (m_allGpuState.pFramebuffer != nullptr))
        {
            ClearImageAttachments(attachmentCount, pAttachments, rectCount, pRects);
        }
        else
        {
            ClearBoundAttachments(attachmentCount, pAttachments, rectCount, pRects);
        }
    }
}

// =====================================================================================================================
// Clears a set of attachments in the current dynamic rendering pass.
void CmdBuffer::ClearDynamicRenderingImages(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    // Note: Bound target clears are pipelined by the HW, so we do not have to insert any barriers
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRects = EstimateMaxObjectsOnVirtualStack(sizeof(*pRects));

    for (uint32_t idx = 0; idx < attachmentCount; ++idx)
    {
        const VkClearAttachment& clearInfo = pAttachments[idx];

        // Detect if color clear or depth clear
        if ((clearInfo.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
        {
            const DynamicRenderingAttachments& attachment =
                m_allGpuState.dynamicRenderingInstance.colorAttachments[clearInfo.colorAttachment];

            // Clear only if the referenced attachment index is active
            if ((attachment.pImageView != nullptr) && (attachment.pImageView->GetImage() != nullptr))
            {
                const Image* pImage = attachment.pImageView->GetImage();

                const Pal::SwizzledFormat palFormat = VkToPalFormat(attachment.attachmentFormat,
                                                                    m_pDevice->GetRuntimeSettings());

                Util::Vector<Pal::Box, 8, VirtualStackFrame> clearBoxes{ &virtStackFrame };
                Util::Vector<Pal::SubresRange, 8, VirtualStackFrame> clearSubresRanges{ &virtStackFrame };

                auto       rectBatch  = Util::Min(rectCount, maxRects);
                const auto palResult1 = clearBoxes.Reserve(rectBatch);
                const auto palResult2 = clearSubresRanges.Reserve(rectBatch);

                if ((palResult1 == Pal::Result::Success) &&
                    (palResult2 == Pal::Result::Success))
                {
                    for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                    {
                        // Obtain the baseArrayLayer of the image view to apply it when clearing the image itself.
                        const uint32_t zOffset = static_cast<uint32_t>(attachment.pImageView->GetZRange().offset);

                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        CreateClearRegions(
                            rectCount,
                            (pRects + rectIdx),
                            m_allGpuState.dynamicRenderingInstance.viewMask,
                            zOffset,
                            &clearBoxes);

                        CreateClearSubresRanges(
                            attachment.pImageView,
                            clearInfo,
                            rectCount,
                            pRects + rectIdx,
                            m_allGpuState.dynamicRenderingInstance.viewMask,
                            &clearSubresRanges);

                         PalCmdClearColorImage(
                             *pImage,
                             attachment.imageLayout,
                             VkToPalClearColor(clearInfo.clearValue.color, palFormat),
                             palFormat,
                             clearSubresRanges.NumElements(),
                             clearSubresRanges.Data(),
                             clearBoxes.NumElements(),
                             clearBoxes.Data(),
                             Pal::ClearColorImageFlags::ColorClearAutoSync);
                    }
                }
                else
                {
                    m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
        else
        {
            const DynamicRenderingAttachments& depthAttachment   =
                m_allGpuState.dynamicRenderingInstance.depthAttachment;
            const DynamicRenderingAttachments& stencilAttachment =
                m_allGpuState.dynamicRenderingInstance.stencilAttachment;

            // Depth and Stencil Views are the same if both exist
            Pal::ImageLayout imageLayout       = {};
            const ImageView* pDepthStencilView = nullptr;

            if ((depthAttachment.pImageView != nullptr) &&
                ((clearInfo.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0))
            {
                pDepthStencilView = depthAttachment.pImageView;
                imageLayout       = depthAttachment.imageLayout;
            }
            else if ((stencilAttachment.pImageView != nullptr) &&
                     ((clearInfo.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0))
            {
                pDepthStencilView = stencilAttachment.pImageView;
                imageLayout       = stencilAttachment.imageLayout;
            }

            // Clear only if the referenced attachment index is active
            if (pDepthStencilView != nullptr)
            {
                Util::Vector<Pal::Rect, 8, VirtualStackFrame> clearRects{ &virtStackFrame };
                Util::Vector<Pal::SubresRange, 8, VirtualStackFrame> clearSubresRanges{ &virtStackFrame };

                auto       rectBatch = Util::Min((rectCount * MaxPalDepthAspectsPerMask), maxRects);
                const auto palResult1 = clearRects.Reserve(rectBatch);
                const auto palResult2 = clearSubresRanges.Reserve(rectBatch);

                if ((palResult1 == Pal::Result::Success) &&
                    (palResult2 == Pal::Result::Success))
                {
                    ValidateSamplePattern(pDepthStencilView->GetImage()->GetImageSamples(), nullptr);

                    for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                    {
                        // Obtain the baseArrayLayer of the image view to apply it when clearing the image itself.
                        const uint32_t zOffset = static_cast<uint32_t>(pDepthStencilView->GetZRange().offset);

                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        CreateClearRects(
                            rectCount,
                            (pRects + rectIdx),
                            &clearRects);

                        CreateClearSubresRanges(
                            pDepthStencilView,
                            clearInfo,
                            rectCount,
                            pRects + rectIdx,
                            m_allGpuState.dynamicRenderingInstance.viewMask,
                            &clearSubresRanges);

                        PalCmdClearDepthStencil(
                            *pDepthStencilView->GetImage(),
                            imageLayout,
                            imageLayout,
                            VkToPalClearDepth(clearInfo.clearValue.depthStencil.depth),
                            clearInfo.clearValue.depthStencil.stencil,
                            clearSubresRanges.NumElements(),
                            clearSubresRanges.Data(),
                            clearRects.NumElements(),
                            clearRects.Data(),
                            Pal::ClearDepthStencilFlags::DsClearAutoSync);
                    }
                }
                else
                {
                    m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
    }
}

// =====================================================================================================================
// Clears a set of attachments in the current renderpass using PAL's CmdClearBound*Targets commands.
void CmdBuffer::ClearDynamicRenderingBoundAttachments(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    // Note: Bound target clears are pipelined by the HW, so we do not have to insert any barriers
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    Util::Vector<Pal::ClearBoundTargetRegion, 8, VirtualStackFrame> clearRegions{ &virtStackFrame };
    Util::Vector<Pal::BoundColorTarget, 8, VirtualStackFrame> colorTargets{ &virtStackFrame };

    const auto maxRects   = EstimateMaxObjectsOnVirtualStack(sizeof(*pRects));
    auto       rectBatch  = Util::Min(rectCount, maxRects);
    const auto palResult1 = clearRegions.Reserve(rectBatch);
    const auto palResult2 = colorTargets.Reserve(attachmentCount);

    m_recordingResult = ((palResult1 == Pal::Result::Success) &&
                         (palResult2 == Pal::Result::Success)) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;

    if (m_recordingResult == VK_SUCCESS)
    {
        for (uint32_t idx = 0; idx < attachmentCount; ++idx)
        {
            const VkClearAttachment& clearInfo = pAttachments[idx];

            // Detect if color clear or depth clear
            if ((clearInfo.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
            {
                // Fill in bound target information for this target, but don't clear yet
                const uint32_t tgtIdx = clearInfo.colorAttachment;

                // Clear only if the attachment reference is active
                if (tgtIdx < m_allGpuState.dynamicRenderingInstance.colorAttachmentCount)
                {
                    const auto& attachment = m_allGpuState.dynamicRenderingInstance.colorAttachments[tgtIdx];

                    if (attachment.attachmentFormat != VK_FORMAT_UNDEFINED)
                    {
                        Pal::BoundColorTarget target = {};

                        target.targetIndex    = tgtIdx;
                        target.swizzledFormat =
                            VkToPalFormat(attachment.attachmentFormat, m_pDevice->GetRuntimeSettings());
                        target.samples        = attachment.rasterizationSamples;
                        target.fragments      = attachment.rasterizationSamples;
                        target.clearValue     = VkToPalClearColor(clearInfo.clearValue.color, target.swizzledFormat);

                        colorTargets.PushBack(target);
                    }
                }
            }
            else // Depth-stencil clear
            {
                Pal::DepthStencilSelectFlags selectFlags = {};

                selectFlags.depth   = ((clearInfo.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0);
                selectFlags.stencil = ((clearInfo.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0);

                DbgBarrierPreCmd(DbgBarrierClearDepth);

                for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                {
                    rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                    uint32_t viewMask = m_allGpuState.dynamicRenderingInstance.viewMask;

                    CreateClearRegions(
                        rectBatch,
                        pRects + rectIdx,
                        viewMask,
                        0u,
                        &clearRegions);

                    // Clear the bound depth stencil target immediately
                    PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundDepthStencilTargets(
                        VkToPalClearDepth(clearInfo.clearValue.depthStencil.depth),
                        clearInfo.clearValue.depthStencil.stencil,
                        StencilWriteMaskFull,
                        m_allGpuState.dynamicRenderingInstance.depthAttachment.rasterizationSamples,
                        m_allGpuState.dynamicRenderingInstance.depthAttachment.rasterizationSamples,
                        selectFlags,
                        clearRegions.NumElements(),
                        clearRegions.Data());
                }

                DbgBarrierPostCmd(DbgBarrierClearDepth);
            }
        }

        if (colorTargets.NumElements() > 0)
        {
            DbgBarrierPreCmd(DbgBarrierClearColor);

            for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
            {
                rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                uint32_t viewMask = m_allGpuState.dynamicRenderingInstance.viewMask;

                CreateClearRegions(
                    rectBatch,
                    pRects + rectIdx,
                    viewMask,
                    0u,
                    &clearRegions);

                // Clear the bound color targets
                PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundColorTargets(
                    colorTargets.NumElements(),
                    colorTargets.Data(),
                    clearRegions.NumElements(),
                    clearRegions.Data());
            }

            DbgBarrierPostCmd(DbgBarrierClearColor);
        }
    }
}

// =====================================================================================================================
// Clears a set of attachments in the current subpass using PAL's CmdClearBound*Targets commands.
void CmdBuffer::ClearBoundAttachments(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    // Note: Bound target clears are pipelined by the HW, so we do not have to insert any barriers

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Get the current renderpass and subpass
    const RenderPass* pRenderPass = m_allGpuState.pRenderPass;
    const uint32_t subpass        = m_renderPassInstance.subpass;

    Util::Vector<Pal::ClearBoundTargetRegion, 8, VirtualStackFrame> clearRegions { &virtStackFrame };
    Util::Vector<Pal::BoundColorTarget,       8, VirtualStackFrame> colorTargets { &virtStackFrame };

    const auto maxRects   = EstimateMaxObjectsOnVirtualStack(sizeof(*pRects));
    auto       rectBatch  = Util::Min(rectCount, maxRects);
    const auto palResult1 = clearRegions.Reserve(rectBatch);
    const auto palResult2 = colorTargets.Reserve(attachmentCount);

    m_recordingResult = ((palResult1 == Pal::Result::Success) &&
                         (palResult2 == Pal::Result::Success)) ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;

    if (m_recordingResult == VK_SUCCESS)
    {
        for (uint32_t idx = 0; idx < attachmentCount; ++idx)
        {
            const VkClearAttachment& clearInfo = pAttachments[idx];

            // Detect if color clear or depth clear
            if ((clearInfo.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
            {
                // Get the corresponding color reference in the current subpass
                const AttachmentReference& colorRef = pRenderPass->GetSubpassColorReference(
                    subpass, clearInfo.colorAttachment);

                // Clear only if the attachment reference is active
                if (colorRef.attachment != VK_ATTACHMENT_UNUSED)
                {
                    // Fill in bound target information for this target, but don't clear yet
                    const uint32_t tgtIdx = clearInfo.colorAttachment;

                    Pal::BoundColorTarget target = {};
                    target.targetIndex    = tgtIdx;
                    target.swizzledFormat = VkToPalFormat(pRenderPass->GetColorAttachmentFormat(subpass, tgtIdx),
                                                          m_pDevice->GetRuntimeSettings());
                    target.samples        = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
                    target.fragments      = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
                    target.clearValue     = VkToPalClearColor(clearInfo.clearValue.color, target.swizzledFormat);

                    colorTargets.PushBack(target);
                }
            }
            else // Depth-stencil clear
            {
                // Get the corresponding color reference in the current subpass
                const AttachmentReference& depthStencilRef = pRenderPass->GetSubpassDepthStencilReference(subpass);

                // Clear only if the attachment reference is active
                if (depthStencilRef.attachment != VK_ATTACHMENT_UNUSED)
                {
                    Pal::DepthStencilSelectFlags selectFlags = {};

                    selectFlags.depth   = ((clearInfo.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0);
                    selectFlags.stencil = ((clearInfo.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0);

                    DbgBarrierPreCmd(DbgBarrierClearDepth);

                    for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                    {
                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        uint32_t viewMask = pRenderPass->GetViewMask(subpass);

                        CreateClearRegions(
                            rectBatch,
                            pRects + rectIdx,
                            viewMask,
                            0u,
                            &clearRegions);

                        // Clear the bound depth stencil target immediately
                        PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundDepthStencilTargets(
                            VkToPalClearDepth(clearInfo.clearValue.depthStencil.depth),
                            clearInfo.clearValue.depthStencil.stencil,
                            StencilWriteMaskFull,
                            pRenderPass->GetDepthStencilAttachmentSamples(subpass),
                            pRenderPass->GetDepthStencilAttachmentSamples(subpass),
                            selectFlags,
                            clearRegions.NumElements(),
                            clearRegions.Data());
                    }

                    DbgBarrierPostCmd(DbgBarrierClearDepth);
                }
            }
        }

        if (colorTargets.NumElements() > 0)
        {
            DbgBarrierPreCmd(DbgBarrierClearColor);

            for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
            {
                rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                uint32_t viewMask = pRenderPass->GetViewMask(subpass);

                CreateClearRegions(
                    rectBatch,
                    pRects + rectIdx,
                    viewMask,
                    0u,
                    &clearRegions);

                // Clear the bound color targets
                PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundColorTargets(
                    colorTargets.NumElements(),
                    colorTargets.Data(),
                    clearRegions.NumElements(),
                    clearRegions.Data());
            }

            DbgBarrierPostCmd(DbgBarrierClearColor);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdClearColorImage(
    const Image&               image,
    Pal::ImageLayout           imageLayout,
    const Pal::ClearColor&     color,
    const Pal::SwizzledFormat& clearFormat,
    uint32_t                   rangeCount,
    const Pal::SubresRange*    pRanges,
    uint32_t                   boxCount,
    const Pal::Box*            pBoxes,
    uint32_t                   flags)
{
    DbgBarrierPreCmd(DbgBarrierClearColor);

    bool bltMsaaState = PreBltBindMsaaState(image);

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdClearColorImage(
            *image.PalImage(deviceIdx),
            imageLayout,
            color,
            clearFormat,
            rangeCount,
            pRanges,
            boxCount,
            pBoxes,
            flags);
    }
    while (deviceGroup.IterateNext());

    PostBltRestoreMsaaState(bltMsaaState);

    DbgBarrierPostCmd(DbgBarrierClearColor);
}

// =====================================================================================================================
void CmdBuffer::PalCmdClearDepthStencil(
    const Image&            image,
    Pal::ImageLayout        depthLayout,
    Pal::ImageLayout        stencilLayout,
    float                   depth,
    uint8_t                 stencil,
    uint32_t                rangeCount,
    const Pal::SubresRange* pRanges,
    uint32_t                rectCount,
    const Pal::Rect*        pRects,
    uint32_t                flags)
{
    DbgBarrierPreCmd(DbgBarrierClearDepth);

    bool bltMsaaState = PreBltBindMsaaState(image);

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdClearDepthStencil(
            *image.PalImage(deviceIdx),
            depthLayout,
            stencilLayout,
            depth,
            stencil,
            StencilWriteMaskFull,
            rangeCount,
            pRanges,
            rectCount,
            pRects,
            flags);
    }
    while (deviceGroup.IterateNext());

    PostBltRestoreMsaaState(bltMsaaState);

    DbgBarrierPostCmd(DbgBarrierClearDepth);
}

// =====================================================================================================================
template <typename EventContainer_T>
void CmdBuffer::PalCmdResetEvent(
    EventContainer_T*       pEvent,
    Pal::HwPipePoint        resetPoint)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdResetEvent(*pEvent->PalEvent(deviceIdx), resetPoint);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
template <typename EventContainer_T>
void CmdBuffer::PalCmdSetEvent(
    EventContainer_T*       pEvent,
    Pal::HwPipePoint        setPoint)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdSetEvent(*pEvent->PalEvent(deviceIdx), setPoint);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdResolveImage(
    const Image&                   srcImage,
    Pal::ImageLayout               srcImageLayout,
    const Image&                   dstImage,
    Pal::ImageLayout               dstImageLayout,
    Pal::ResolveMode               resolveMode,
    uint32_t                       regionCount,
    const Pal::ImageResolveRegion* pRegions,
    uint32_t                       deviceMask)
{
    DbgBarrierPreCmd(DbgBarrierResolve);

    bool bltMsaaState = PreBltBindMsaaState(srcImage);

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdResolveImage(
                    *srcImage.PalImage(deviceIdx),
                    srcImageLayout,
                    *dstImage.PalImage(deviceIdx),
                    dstImageLayout,
                    resolveMode,
                    regionCount,
                    pRegions,
                    0);
    }
    while (deviceGroup.IterateNext());

    PostBltRestoreMsaaState(bltMsaaState);

    DbgBarrierPostCmd(DbgBarrierResolve);
}

// =====================================================================================================================
// Clears a set of attachments in the current subpass using PAL's CmdClear*Image() commands.
void CmdBuffer::ClearImageAttachments(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Get the current renderpass and subpass
    const RenderPass* pRenderPass = m_allGpuState.pRenderPass;
    const uint32_t    subpass     = m_renderPassInstance.subpass;
    const auto        maxRects    = EstimateMaxObjectsOnVirtualStack(sizeof(*pRects));

    // Go through each of the clear attachment infos
    for (uint32_t idx = 0; idx < attachmentCount; ++idx)
    {
        const VkClearAttachment& clearInfo = pAttachments[idx];

        // Detect if color clear or depth clear
        if ((clearInfo.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
        {
            // Get the color target index (subpass color reference index)
            const uint32_t targetIdx = clearInfo.colorAttachment;

            // Get the corresponding color reference in the current subpass
            const AttachmentReference& colorRef = pRenderPass->GetSubpassColorReference(subpass, targetIdx);

            // Get the referenced attachment index in the framebuffer
            const uint32_t attachmentIdx = colorRef.attachment;

            // Clear only if the referenced attachment index is active
            if (attachmentIdx != VK_ATTACHMENT_UNUSED)
            {
                // Get the matching framebuffer attachment
                const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

                // Get the layout that this color attachment is currently in within the render pass
                const Pal::ImageLayout targetLayout = RPGetAttachmentLayout(attachmentIdx, 0);

                Util::Vector<Pal::Box,         8, VirtualStackFrame> clearBoxes        { &virtStackFrame };
                Util::Vector<Pal::SubresRange, 8, VirtualStackFrame> clearSubresRanges { &virtStackFrame };

                auto       rectBatch  = Util::Min(rectCount, maxRects);
                const auto palResult1 = clearBoxes.Reserve(rectBatch);
                const auto palResult2 = clearSubresRanges.Reserve(rectBatch);

                if ((palResult1 == Pal::Result::Success) &&
                    (palResult2 == Pal::Result::Success))
                {
                    for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                    {
                        // Obtain the baseArrayLayer of the image view to apply it when clearing the image itself.
                        const uint32_t zOffset = static_cast<uint32_t>(attachment.pView->GetZRange().offset);

                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        uint32_t viewMask = pRenderPass->GetViewMask(subpass);

                        CreateClearRegions(
                            rectCount,
                            pRects + rectIdx,
                            viewMask,
                            zOffset,
                            &clearBoxes);

                        CreateClearSubresRanges(
                            attachment, clearInfo,
                            rectCount, pRects + rectIdx,
                            *pRenderPass, subpass,
                            &clearSubresRanges);

                        PalCmdClearColorImage(
                            *attachment.pImage,
                            targetLayout,
                            VkToPalClearColor(clearInfo.clearValue.color, attachment.viewFormat),
                            attachment.viewFormat,
                            clearSubresRanges.NumElements(),
                            clearSubresRanges.Data(),
                            clearBoxes.NumElements(),
                            clearBoxes.Data(),
                            Pal::ClearColorImageFlags::ColorClearAutoSync);
                    }
                }
                else
                {
                    m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
        else // Depth-stencil clear
        {
            // Get the depth-stencil reference of the current subpass
            const AttachmentReference& depthStencilRef = pRenderPass->GetSubpassDepthStencilReference(subpass);

            // Get the referenced attachment index in the framebuffer
            const uint32_t attachmentIdx = depthStencilRef.attachment;

            // Clear only if the referenced attachment index is active
            if (attachmentIdx != VK_ATTACHMENT_UNUSED)
            {
                // Get the matching framebuffer attachment
                const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

                // Get the layout(s) that this attachment is currently in within the render pass
                const Pal::ImageLayout depthLayout   = RPGetAttachmentLayout(attachmentIdx, 0);
                const Pal::ImageLayout stencilLayout = RPGetAttachmentLayout(attachmentIdx, 1);

                Util::Vector<Pal::Rect,        8, VirtualStackFrame> clearRects        { &virtStackFrame };
                Util::Vector<Pal::SubresRange, 8, VirtualStackFrame> clearSubresRanges { &virtStackFrame };

                auto       rectBatch  = Util::Min(rectCount, maxRects);
                const auto palResult1 = clearRects.Reserve(rectBatch);
                const auto palResult2 = clearSubresRanges.Reserve(rectBatch);

                if ((palResult1 == Pal::Result::Success) &&
                    (palResult2 == Pal::Result::Success))
                {
                    ValidateSamplePattern(attachment.pImage->GetImageSamples(), nullptr);

                    for (uint32_t rectIdx = 0; rectIdx < rectCount; rectIdx += rectBatch)
                    {
                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        CreateClearRects(
                            rectCount, pRects + rectIdx,
                            &clearRects);

                        CreateClearSubresRanges(
                            attachment, clearInfo,
                            rectCount, pRects + rectIdx,
                            *pRenderPass, subpass,
                            &clearSubresRanges);

                        PalCmdClearDepthStencil(
                            *attachment.pImage,
                            depthLayout,
                            stencilLayout,
                            VkToPalClearDepth(clearInfo.clearValue.depthStencil.depth),
                            clearInfo.clearValue.depthStencil.stencil,
                            clearSubresRanges.NumElements(),
                            clearSubresRanges.Data(),
                            clearRects.NumElements(),
                            clearRects.Data(),
                            Pal::ClearDepthStencilFlags::DsClearAutoSync);
                    }
                }
                else
                {
                    m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }
        }
    }
}

// =====================================================================================================================
template<typename ImageResolveType>
void CmdBuffer::ResolveImage(
    VkImage                 srcImage,
    VkImageLayout           srcImageLayout,
    VkImage                 destImage,
    VkImageLayout           destImageLayout,
    uint32_t                rectCount,
    const ImageResolveType* pRects)
{
    PalCmdSuspendPredication(true);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRects  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRects)), MaxRangePerAttachment);
    auto       rectBatch = Util::Min(rectCount * MaxRangePerAttachment, maxRects);

    // Allocate space to store image resolve regions (we need a separate region per PAL aspect)
    Pal::ImageResolveRegion* pPalRegions =
        virtStackFrame.AllocArray<Pal::ImageResolveRegion>(rectBatch);

    if (m_pDevice->GetRuntimeSettings().overrideUndefinedLayoutToTransferSrcOptimal)
    {
        if (srcImageLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        }
    }

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage             = Image::ObjectFromHandle(srcImage);
        const Image* const pDstImage             = Image::ObjectFromHandle(destImage);
        const Pal::SwizzledFormat srcFormat      = VkToPalFormat(pSrcImage->GetFormat(),
                                                                 m_pDevice->GetRuntimeSettings());
        const Pal::ImageLayout palSrcImageLayout = pSrcImage->GetBarrierPolicy().GetTransferLayout(
            srcImageLayout, GetQueueFamilyIndex());
        const Pal::ImageLayout palDestImageLayout = pDstImage->GetBarrierPolicy().GetTransferLayout(
            destImageLayout, GetQueueFamilyIndex());

        // If ever permitted by the spec, pQuadSamplePattern must be specified because the source image was created with
        // sampleLocsAlwaysKnown set.
        VK_ASSERT(pSrcImage->IsDepthStencilFormat() == false);

        for (uint32_t rectIdx = 0; rectIdx < rectCount;)
        {
            uint32_t palRegionCount = 0;

            while ((rectIdx < rectCount) &&
                   (palRegionCount <= (rectBatch - MaxPalAspectsPerMask)))
            {
                // We expect MSAA images to never have mipmaps
                VK_ASSERT(pRects[rectIdx].srcSubresource.mipLevel == 0);

                VkToPalImageResolveRegion(
                    pRects[rectIdx], srcFormat, pSrcImage->GetArraySize(),
                    pDstImage->TreatAsSrgb(), pPalRegions, &palRegionCount);

                ++rectIdx;
            }

            PalCmdResolveImage(
                *pSrcImage,
                palSrcImageLayout,
                *pDstImage,
                palDestImageLayout,
                Pal::ResolveMode::Average,
                palRegionCount,
                pPalRegions,
                m_curDeviceMask);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    PalCmdSuspendPredication(false);
}

// =====================================================================================================================
// Implementation of vkCmdSetEvent()
void CmdBuffer::SetEvent(
    VkEvent                       event,
    PipelineStageFlags            stageMask)
{
    DbgBarrierPreCmd(DbgBarrierSetResetEvent);

    PalCmdSetEvent(Event::ObjectFromHandle(event), VkToPalSrcPipePoint(stageMask));

    DbgBarrierPostCmd(DbgBarrierSetResetEvent);
}

// =====================================================================================================================
// Implementation of vkCmdSetEvent2()
void CmdBuffer::SetEvent2(
    VkEvent                    event,
    const VkDependencyInfoKHR* pDependencyInfo)
{
    DbgBarrierPreCmd(DbgBarrierSetResetEvent);

    if (m_flags.useSplitReleaseAcquire)
    {
        ExecuteAcquireRelease(1,
                              &event,
                              1,
                              pDependencyInfo,
                              Release,
                              RgpBarrierExternalCmdWaitEvents);
    }
    else
    {
        PipelineStageFlags stageMask = 0;

        for(uint32_t i = 0; i < pDependencyInfo->memoryBarrierCount; i++)
        {
            stageMask |= pDependencyInfo->pMemoryBarriers[i].srcStageMask;
        }

        for (uint32_t i = 0; i < pDependencyInfo->bufferMemoryBarrierCount; i++)
        {
            stageMask |= pDependencyInfo->pBufferMemoryBarriers[i].srcStageMask;
        }

        for (uint32_t i = 0; i < pDependencyInfo->imageMemoryBarrierCount; i++)
        {
            stageMask |= pDependencyInfo->pImageMemoryBarriers[i].srcStageMask;
        }

        PalCmdSetEvent(Event::ObjectFromHandle(event), VkToPalSrcPipePoint(stageMask));
    }

    DbgBarrierPostCmd(DbgBarrierSetResetEvent);
}

// =====================================================================================================================
// Returns attachment's PAL subresource ranges defined by clearInfo for Dynamic Rendering LoadOp Clear.
// When multiview is enabled, layer ranges are modified according active views during a renderpass.
Util::Vector<Pal::SubresRange, MaxPalAspectsPerMask * Pal::MaxViewInstanceCount, Util::GenericAllocator>
LoadOpClearSubresRanges(
    const uint32_t&         viewMask,
    const Pal::SubresRange& subresRange)
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Util::Vector<Pal::SubresRange, MaxPalAspectsPerMask * Pal::MaxViewInstanceCount, Util::GenericAllocator> clearSubresRanges{ nullptr };

    if (viewMask > 0)
    {
        const auto layerRanges = RangesOfOnesInBitMask(viewMask);

        for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
        {
            clearSubresRanges.PushBack(subresRange);
            clearSubresRanges.Back().startSubres.arraySlice += layerRangeIt.Get().offset;
            clearSubresRanges.Back().numSlices               = layerRangeIt.Get().extent;
        }
    }
    else
    {
        clearSubresRanges.PushBack(subresRange);
    }

    return clearSubresRanges;
}

// =====================================================================================================================
// Clear Color for VK_KHR_dynamic_rendering
void CmdBuffer::LoadOpClearColor(
    const Pal::Rect*          pDeviceGroupRenderArea,
    const VkRenderingInfoKHR* pRenderingInfo)
{
    for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; ++i)
    {
        const VkRenderingAttachmentInfoKHR& attachmentInfo = pRenderingInfo->pColorAttachments[i];

        if (attachmentInfo.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
        {
            // Get the image view from the attachment info
            const ImageView* const pImageView = ImageView::ObjectFromHandle(attachmentInfo.imageView);
            if (pImageView != VK_NULL_HANDLE)
            {
                // Get the attachment image
                const Image* pImage = pImageView->GetImage();

                // Convert the clear color to the format of the attachment view
                Pal::SwizzledFormat clearFormat = VkToPalFormat(
                    pImageView->GetViewFormat(),
                    m_pDevice->GetRuntimeSettings());
                Pal::ClearColor clearColor = VkToPalClearColor(
                    attachmentInfo.clearValue.color,
                    clearFormat);

                // Get subres range from the image view
                Pal::SubresRange subresRange = {};
                pImageView->GetFrameBufferAttachmentSubresRange(&subresRange);

                // Override the number of slices with layerCount from pBeginRendering
                subresRange.numSlices = pRenderingInfo->layerCount;

                const auto clearSubresRanges = LoadOpClearSubresRanges(
                    pRenderingInfo->viewMask,
                    subresRange);

                // Clear Layout
                const Pal::ImageLayout clearLayout = pImage->GetBarrierPolicy().GetAspectLayout(
                    attachmentInfo.imageLayout,
                    subresRange.startSubres.plane,
                    GetQueueFamilyIndex(),
                    pImage->GetFormat());

                utils::IterateMask deviceGroup(GetDeviceMask());

                do
                {
                    const uint32_t deviceIdx = deviceGroup.Index();

                    // Clear Box
                    Pal::Box clearBox = BuildClearBox(
                        pDeviceGroupRenderArea[deviceIdx],
                        *pImageView);

                    PalCmdBuffer(deviceIdx)->CmdClearColorImage(
                        *pImage->PalImage(deviceIdx),
                        clearLayout,
                        clearColor,
                        clearFormat,
                        clearSubresRanges.NumElements(),
                        clearSubresRanges.Data(),
                        1,
                        &clearBox,
                        Pal::ColorClearAutoSync);
                }
                while (deviceGroup.IterateNext());
            }
        }
    }
}

// =====================================================================================================================
// Clear Depth Stencil for VK_KHR_dynamic_rendering
void CmdBuffer::LoadOpClearDepthStencil(
    const Pal::Rect*          pDeviceGroupRenderArea,
    const VkRenderingInfoKHR* pRenderingInfo)
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Util::Vector<Pal::SubresRange, MaxPalAspectsPerMask * Pal::MaxViewInstanceCount, Util::GenericAllocator> clearSubresRanges{ nullptr };

    const Image* pDepthStencilImage = nullptr;

    Pal::SubresRange subresRange   = {};
    Pal::ImageLayout depthLayout   = {};
    Pal::ImageLayout stencilLayout = {};

    float clearDepth   = 0.0f;
    uint8 clearStencil = 0;

    const VkRenderingAttachmentInfoKHR* pDepthAttachmentInfo   = pRenderingInfo->pDepthAttachment;
    const VkRenderingAttachmentInfoKHR* pStencilAttachmentInfo = pRenderingInfo->pStencilAttachment;

    if ((pStencilAttachmentInfo != nullptr) &&
        (pStencilAttachmentInfo->imageView != VK_NULL_HANDLE))
    {
        const ImageView* const pStencilImageView = ImageView::ObjectFromHandle(pStencilAttachmentInfo->imageView);

        if (pStencilImageView != VK_NULL_HANDLE)
        {
            pDepthStencilImage = pStencilImageView->GetImage();

            GetImageLayout(
                pStencilAttachmentInfo->imageView,
                pStencilAttachmentInfo->imageLayout,
                VK_IMAGE_ASPECT_STENCIL_BIT,
                &subresRange,
                &stencilLayout);

            if (pStencilAttachmentInfo->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                clearSubresRanges.PushBack(subresRange);
                clearStencil = pStencilAttachmentInfo->clearValue.depthStencil.stencil;
            }
        }
    }

    if ((pDepthAttachmentInfo != nullptr) &&
        (pDepthAttachmentInfo->imageView != VK_NULL_HANDLE))
    {
        const ImageView* const pDepthImageView = ImageView::ObjectFromHandle(pDepthAttachmentInfo->imageView);

        if (pDepthImageView != VK_NULL_HANDLE)
        {
            pDepthStencilImage = pDepthImageView->GetImage();

            GetImageLayout(
                pDepthAttachmentInfo->imageView,
                pDepthAttachmentInfo->imageLayout,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                &subresRange,
                &depthLayout);

            if (pDepthAttachmentInfo->loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
            {
                clearSubresRanges.PushBack(subresRange);
                clearDepth = pDepthAttachmentInfo->clearValue.depthStencil.depth;
            }
        }
    }
    else
    {
        depthLayout = stencilLayout;
    }

    if (pDepthStencilImage != nullptr)
    {
        ValidateSamplePattern(pDepthStencilImage->GetImageSamples(), nullptr);

        utils::IterateMask deviceGroup(GetDeviceMask());

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdClearDepthStencil(
                *pDepthStencilImage->PalImage(deviceIdx),
                depthLayout,
                stencilLayout,
                clearDepth,
                clearStencil,
                StencilWriteMaskFull,
                clearSubresRanges.NumElements(),
                clearSubresRanges.Data(),
                1,
                &(pDeviceGroupRenderArea[deviceIdx]),
                Pal::DsClearAutoSync);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
// StoreAttachment for VK_KHR_dynamic_rendering
void CmdBuffer::StoreAttachmentInfo(
    const VkRenderingAttachmentInfoKHR& renderingAttachmentInfo,
    DynamicRenderingAttachments*        pDynamicRenderingAttachement)
{
    const ImageView* const pImageView = ImageView::ObjectFromHandle(renderingAttachmentInfo.imageView);

    if(pImageView != nullptr)
    {
        const Image*           pColorImage = pImageView->GetImage();

        Pal::ImageLayout colorImageLayout = pColorImage->GetAttachmentLayout(
            { renderingAttachmentInfo.imageLayout, 0 },
            0,
            this);

        pDynamicRenderingAttachement->attachmentFormat   = pColorImage->GetFormat();
        pDynamicRenderingAttachement->resolveMode        = renderingAttachmentInfo.resolveMode;
        pDynamicRenderingAttachement->pImageView         = pImageView;
        pDynamicRenderingAttachement->imageLayout        = colorImageLayout;
        pDynamicRenderingAttachement->pResolveImageView  = ImageView::ObjectFromHandle(
                                                            renderingAttachmentInfo.resolveImageView);

        if (pDynamicRenderingAttachement->pResolveImageView != nullptr)
        {
            const Image* pResolveImage = pDynamicRenderingAttachement->pResolveImageView->GetImage();

            if (pResolveImage != nullptr)
            {
                pDynamicRenderingAttachement->resolveImageLayout =
                    pResolveImage->GetAttachmentLayout(
                        { renderingAttachmentInfo.resolveImageLayout, Pal::LayoutResolveDst }, 0, this);
            }
        }
    }
}

// =====================================================================================================================
// vkCmdBeginRendering for VK_KHR_dynamic_rendering
void CmdBuffer::BeginRendering(
    const VkRenderingInfoKHR* pRenderingInfo)
{
    VK_ASSERT(pRenderingInfo != nullptr);

    DbgBarrierPreCmd(DbgBarrierBeginRendering);

    bool isResuming  = (pRenderingInfo->flags & VK_RENDERING_RESUMING_BIT_KHR);
    bool isSuspended = (pRenderingInfo->flags & VK_RENDERING_SUSPENDING_BIT_KHR);

    bool skipEverything = isResuming && m_flags.isRenderingSuspended;
    bool skipClears     = isResuming && (m_flags.isRenderingSuspended == false);

    if (!skipEverything)
    {
        EXTRACT_VK_STRUCTURES_2(
            RENDERING_INFO_KHR,
            RenderingInfoKHR,
            DeviceGroupRenderPassBeginInfo,
            RenderingFragmentShadingRateAttachmentInfoKHR,
            pRenderingInfo,
            RENDER_PASS_BEGIN_INFO,
            DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,
            RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR)

        bool replicateRenderArea = true;

        if (pDeviceGroupRenderPassBeginInfo != nullptr)
        {
            SetDeviceMask(pDeviceGroupRenderPassBeginInfo->deviceMask);

            m_allGpuState.dynamicRenderingInstance.renderAreaCount =
                pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount;

            VK_ASSERT(m_allGpuState.dynamicRenderingInstance.renderAreaCount <= MaxPalDevices);

            VK_ASSERT(m_renderPassInstance.renderAreaCount <= MaxPalDevices);

            if (pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount > 0)
            {
                utils::IterateMask deviceGroup(pDeviceGroupRenderPassBeginInfo->deviceMask);

                VK_ASSERT(m_numPalDevices == pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount);

                do
                {
                    const uint32_t deviceIdx = deviceGroup.Index();

                    const VkRect2D& srcRect  = pDeviceGroupRenderPassBeginInfo->pDeviceRenderAreas[deviceIdx];
                    auto*           pDstRect = &m_allGpuState.dynamicRenderingInstance.renderArea[deviceIdx];

                    *pDstRect = VkToPalRect(srcRect);
                }
                while (deviceGroup.IterateNext());

                replicateRenderArea = false;
            }
        }

        if (replicateRenderArea)
        {
            m_allGpuState.dynamicRenderingInstance.renderAreaCount = m_numPalDevices;

            const auto& srcRect = pRenderingInfo->renderArea;

            for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
            {
                auto* pDstRect = &m_allGpuState.dynamicRenderingInstance.renderArea[deviceIdx];

                *pDstRect = VkToPalRect(srcRect);
            }
        }

        Pal::GlobalScissorParams scissorParams    = {};
        scissorParams.scissorRegion = VkToPalRect(pRenderingInfo->renderArea);

        utils::IterateMask deviceGroup(GetDeviceMask());
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            PalCmdBuffer(deviceIdx)->CmdSetGlobalScissor(scissorParams);
        }
        while (deviceGroup.IterateNext());

        if (!skipClears)
        {
            PalCmdSuspendPredication(true);

            LoadOpClearColor(
                m_allGpuState.dynamicRenderingInstance.renderArea,
                pRenderingInfo);

            LoadOpClearDepthStencil(
                m_allGpuState.dynamicRenderingInstance.renderArea,
                pRenderingInfo);

            PalCmdSuspendPredication(false);
        }

        BindTargets(
            pRenderingInfo,
            pRenderingFragmentShadingRateAttachmentInfoKHR);

        uint32_t numMultiViews = Util::CountSetBits(pRenderingInfo->viewMask);
        uint32_t viewInstanceMask = (numMultiViews > 0) ? pRenderingInfo->viewMask : GetDeviceMask();
        PalCmdBuffer(DefaultDeviceIndex)->CmdSetViewInstanceMask(viewInstanceMask);
    }

    m_allGpuState.dynamicRenderingInstance.viewMask             = pRenderingInfo->viewMask;
    m_allGpuState.dynamicRenderingInstance.colorAttachmentCount = pRenderingInfo->colorAttachmentCount;
    m_allGpuState.dynamicRenderingInstance.enableResolveTarget  = false;

    for (uint32_t i = 0; i < pRenderingInfo->colorAttachmentCount; ++i)
    {
        const VkRenderingAttachmentInfoKHR& colorAttachmentInfo = pRenderingInfo->pColorAttachments[i];

        m_allGpuState.dynamicRenderingInstance.enableResolveTarget |=
            (colorAttachmentInfo.resolveImageView != VK_NULL_HANDLE);

        StoreAttachmentInfo(
            colorAttachmentInfo,
            &m_allGpuState.dynamicRenderingInstance.colorAttachments[i]);
    }

    if (pRenderingInfo->pDepthAttachment != nullptr)
    {
        const VkRenderingAttachmentInfoKHR& depthAttachmentInfo = *pRenderingInfo->pDepthAttachment;

        m_allGpuState.dynamicRenderingInstance.enableResolveTarget |=
            (depthAttachmentInfo.resolveImageView != VK_NULL_HANDLE);

        StoreAttachmentInfo(
            depthAttachmentInfo,
            &m_allGpuState.dynamicRenderingInstance.depthAttachment);
    }

    if (pRenderingInfo->pStencilAttachment != nullptr)
    {
        const VkRenderingAttachmentInfoKHR& stencilAttachmentInfo = *pRenderingInfo->pStencilAttachment;

        m_allGpuState.dynamicRenderingInstance.enableResolveTarget |=
            (stencilAttachmentInfo.resolveImageView != VK_NULL_HANDLE);

        StoreAttachmentInfo(
            stencilAttachmentInfo,
            &m_allGpuState.dynamicRenderingInstance.stencilAttachment);
    }

    m_flags.isRenderingSuspended = isSuspended;

    DbgBarrierPostCmd(DbgBarrierBeginRendering);
}

// =====================================================================================================================
// Call resolve image for VK_KHR_dynamic_rendering
void CmdBuffer::ResolveImage(
    VkImageAspectFlags                 aspectMask,
    const DynamicRenderingAttachments& dynamicRenderingAttachments)
{
    Pal::ImageResolveRegion regions[MaxPalDevices] = {};

    for (uint32_t idx = 0; idx < m_allGpuState.dynamicRenderingInstance.renderAreaCount; idx++)
    {
        const Pal::Rect& renderArea = m_allGpuState.dynamicRenderingInstance.renderArea[idx];
        Pal::SubresRange subresRangeSrc = {};
        Pal::SubresRange subresRangeDst = {};

        dynamicRenderingAttachments.pResolveImageView->GetFrameBufferAttachmentSubresRange(&subresRangeDst);
        dynamicRenderingAttachments.pImageView->GetFrameBufferAttachmentSubresRange(&subresRangeSrc);

        const uint32_t sliceCount = Util::Min(subresRangeSrc.numSlices,
                                              subresRangeDst.numSlices);

        regions[idx].swizzledFormat = Pal::UndefinedSwizzledFormat;
        regions[idx].extent.width   = renderArea.extent.width;
        regions[idx].extent.height  = renderArea.extent.height;
        regions[idx].extent.depth   = 1;
        regions[idx].numSlices      = 1;
        regions[idx].srcOffset.x    = renderArea.offset.x;
        regions[idx].srcOffset.y    = renderArea.offset.y;
        regions[idx].srcOffset.z    = 0;
        regions[idx].dstOffset.x    = renderArea.offset.x;
        regions[idx].dstOffset.y    = renderArea.offset.y;
        regions[idx].dstOffset.z    = 0;
        regions[idx].dstMipLevel    = subresRangeDst.startSubres.mipLevel;
        regions[idx].dstSlice       = subresRangeDst.startSubres.arraySlice;
        regions[idx].numSlices      = sliceCount;

        if ((aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) &&
             dynamicRenderingAttachments.pImageView->GetImage()->HasDepthAndStencil())
        {
            regions[idx].srcPlane = 1;
        }

        if ((aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) &&
             dynamicRenderingAttachments.pResolveImageView->GetImage()->HasDepthAndStencil())
        {
            regions[idx].dstPlane = 1;
        }

        if (Formats::HasDepth(dynamicRenderingAttachments.pImageView->GetViewFormat()))
        {
            regions[idx].pQuadSamplePattern =
                Device::GetDefaultQuadSamplePattern(
                    dynamicRenderingAttachments.pImageView->GetImage()->GetImageSamples());
        }

    }

    PalCmdResolveImage(
        *dynamicRenderingAttachments.pImageView->GetImage(),
        dynamicRenderingAttachments.imageLayout,
        *dynamicRenderingAttachments.pResolveImageView->GetImage(),
        dynamicRenderingAttachments.resolveImageLayout,
        VkToPalResolveMode(dynamicRenderingAttachments.resolveMode),
        m_allGpuState.dynamicRenderingInstance.renderAreaCount,
        regions,
        m_curDeviceMask);
}

// =====================================================================================================================
// For Dynamic Rendering we need to wait for draws to finish before we do resolves.
void CmdBuffer::PostDrawPreResolveSync()
{
    Pal::BarrierInfo barrierInfo = {};
    barrierInfo.waitPoint = Pal::HwPipePreCs;

    const Pal::HwPipePoint pipePoint = Pal::HwPipePostPs;
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints = &pipePoint;

    Pal::BarrierTransition transition = {};
    transition.srcCacheMask = Pal::CoherColorTarget | Pal::CoherDepthStencilTarget;
    transition.dstCacheMask = Pal::CoherShader;

    barrierInfo.transitionCount = 1;
    barrierInfo.pTransitions = &transition;

    PalCmdBuffer(DefaultDeviceIndex)->CmdBarrier(barrierInfo);
}

// =====================================================================================================================
// vkCmdEndRendering for VK_KHR_dynamic_rendering
void CmdBuffer::EndRendering()
{
    DbgBarrierPreCmd(DbgBarrierEndRenderPass);

    // Only do resolves if renderpass isn't suspended and
    // there are resolve targets
    if (m_allGpuState.dynamicRenderingInstance.enableResolveTarget &&
        (m_flags.isRenderingSuspended == false))
    {
        // Sync draws before resolves
        PostDrawPreResolveSync();

        // Resolve Color Images
        for (uint32_t i = 0; i < m_allGpuState.dynamicRenderingInstance.colorAttachmentCount; ++i)
        {
            const DynamicRenderingAttachments& renderingAttachmentInfo =
                m_allGpuState.dynamicRenderingInstance.colorAttachments[i];

            if ((renderingAttachmentInfo.resolveMode != VK_RESOLVE_MODE_NONE) &&
                (renderingAttachmentInfo.pResolveImageView != nullptr))
            {
                ResolveImage(
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    renderingAttachmentInfo);
            }
        }

        // Resolve Depth Image
        if ((m_allGpuState.dynamicRenderingInstance.depthAttachment.resolveMode != VK_RESOLVE_MODE_NONE) &&
            (m_allGpuState.dynamicRenderingInstance.depthAttachment.pResolveImageView != nullptr))
        {
            ResolveImage(
                VK_IMAGE_ASPECT_DEPTH_BIT,
                m_allGpuState.dynamicRenderingInstance.depthAttachment);
        }

        // Resolve Stencil Image
        if ((m_allGpuState.dynamicRenderingInstance.stencilAttachment.resolveMode != VK_RESOLVE_MODE_NONE) &&
            (m_allGpuState.dynamicRenderingInstance.stencilAttachment.pResolveImageView != nullptr))
        {
            ResolveImage(
                VK_IMAGE_ASPECT_STENCIL_BIT,
                m_allGpuState.dynamicRenderingInstance.stencilAttachment);
        }
    }

    // Reset attachment counts at End of Rendering
    m_allGpuState.dynamicRenderingInstance.enableResolveTarget = false;
    m_allGpuState.dynamicRenderingInstance.colorAttachmentCount = 0;
    m_allGpuState.dynamicRenderingInstance.depthAttachment      = {};
    m_allGpuState.dynamicRenderingInstance.stencilAttachment    = {};

    DbgBarrierPostCmd(DbgBarrierEndRenderPass);
}

// =====================================================================================================================
void CmdBuffer::ResetEvent(
    VkEvent                  event,
    PipelineStageFlags       stageMask)
{
    DbgBarrierPreCmd(DbgBarrierSetResetEvent);

    Event* pEvent = Event::ObjectFromHandle(event);

    if (pEvent->IsUseToken())
    {
        pEvent->SetSyncToken(0xFFFFFFFF);
    }
    else
    {
        const Pal::HwPipePoint pipePoint = VkToPalSrcPipePoint(stageMask);

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdResetEvent(*pEvent->PalEvent(deviceIdx), pipePoint);
        }
        while (deviceGroup.IterateNext());
    }

    DbgBarrierPostCmd(DbgBarrierSetResetEvent);
}

// =====================================================================================================================
// Helper function called from ExecuteBarriers
void CmdBuffer::FlushBarriers(
    Pal::BarrierInfo*              pBarrier,
    Pal::BarrierTransition* const  pTransitions,
    const Image**                  pTransitionImages,
    uint32_t                       mainTransitionCount)
{
    pBarrier->transitionCount = mainTransitionCount;
    pBarrier->pTransitions    = pTransitions;

    PalCmdBarrier(pBarrier, pTransitions, pTransitionImages, m_curDeviceMask);

    // Remove any signaled events as we do not want to wait more than once.
    pBarrier->gpuEventWaitCount = 0;
    pBarrier->ppGpuEvents = nullptr;
}

// =====================================================================================================================
// ExecuteBarriers  Called by vkCmdWaitEvents() and vkCmdPipelineBarrier().
void CmdBuffer::ExecuteBarriers(
    VirtualStackFrame*           pVirtStackFrame,
    uint32_t                     memBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers,
    Pal::BarrierInfo*            pBarrier)
{
    // The sum of all memory barriers and execution barriers
    uint32_t barrierCount = memBarrierCount + bufferMemoryBarrierCount + imageMemoryBarrierCount +
                            pBarrier->gpuEventWaitCount + pBarrier->pipePointWaitCount;
    if (barrierCount == 0)
    {
        return;
    }

    constexpr uint32_t MaxTransitionCount = 512;
    constexpr uint32_t MaxLocationCount = 128;

    pBarrier->globalSrcCacheMask = 0u;
    pBarrier->globalDstCacheMask = 0u;

    Pal::BarrierTransition* pTransitions = pVirtStackFrame->AllocArray<Pal::BarrierTransition>(MaxTransitionCount);
    Pal::BarrierTransition* pNextMain    = pTransitions;

    if (pTransitions == nullptr)
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;

        return;
    }

    const Image** pTransitionImages = (m_numPalDevices > 1) && (imageMemoryBarrierCount > 0) ?
        pVirtStackFrame->AllocArray<const Image*>(MaxTransitionCount) : nullptr;

    for (uint32_t i = 0; i < memBarrierCount; ++i)
    {
        m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
            pMemoryBarriers[i].srcAccessMask,
            pMemoryBarriers[i].dstAccessMask,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            pNextMain);

        pNextMain->imageInfo.pImage = nullptr;
        VK_ASSERT(pMemoryBarriers[i].pNext == nullptr);

        ++pNextMain;

        const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

        if (MaxPalAspectsPerMask + mainTransitionCount > MaxTransitionCount)
        {
            FlushBarriers(pBarrier, pTransitions, nullptr, mainTransitionCount);

            pNextMain = pTransitions;
        }
    }

    for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i)
    {
        const Buffer* pBuffer = Buffer::ObjectFromHandle(pBufferMemoryBarriers[i].buffer);

        pBuffer->GetBarrierPolicy().ApplyBufferMemoryBarrier<VkBufferMemoryBarrier>(
            GetQueueFamilyIndex(),
            pBufferMemoryBarriers[i],
            pNextMain);

        pNextMain->imageInfo.pImage = nullptr;

        VK_ASSERT(pBufferMemoryBarriers[i].pNext == nullptr);

        ++pNextMain;

        const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

        if (MaxPalAspectsPerMask + mainTransitionCount > MaxTransitionCount)
        {
            FlushBarriers(pBarrier, pTransitions, nullptr, mainTransitionCount);

            pNextMain = pTransitions;
        }
    }

    uint32_t locationIndex = 0;
    uint32_t locationCount = (imageMemoryBarrierCount > MaxLocationCount) ? MaxLocationCount : imageMemoryBarrierCount;
    Pal::MsaaQuadSamplePattern* pLocations = imageMemoryBarrierCount > 0
                                           ? pVirtStackFrame->AllocArray<Pal::MsaaQuadSamplePattern>(locationCount)
                                           : nullptr;

    for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i)
    {
        const Image*           pImage                 = Image::ObjectFromHandle(pImageMemoryBarriers[i].image);
        VkFormat               format                 = pImage->GetFormat();
        Pal::BarrierTransition barrierTransition      = { 0 };
        bool                   layoutChanging         = false;
        Pal::ImageLayout oldLayouts[MaxPalAspectsPerMask];
        Pal::ImageLayout newLayouts[MaxPalAspectsPerMask];

        pImage->GetBarrierPolicy().ApplyImageMemoryBarrier<VkImageMemoryBarrier>(
            GetQueueFamilyIndex(),
            pImageMemoryBarriers[i],
            &barrierTransition,
            &layoutChanging,
            oldLayouts,
            newLayouts,
            true);

        pNextMain->imageInfo.pImage = nullptr;

        uint32_t         layoutIdx     = 0;
        uint32_t         palRangeIdx   = 0;
        uint32_t         palRangeCount = 0;
        Pal::SubresRange palRanges[MaxPalAspectsPerMask];

        VkToPalSubresRange(
            format,
            pImageMemoryBarriers[i].subresourceRange,
            pImage->GetMipLevels(),
            pImage->GetArraySize(),
            palRanges,
            &palRangeCount,
            m_pDevice->GetRuntimeSettings());

        if (layoutChanging && Formats::HasStencil(format))
        {
            if (palRangeCount == MaxPalDepthAspectsPerMask)
            {
                // Find the subset of an images subres ranges that need to be transitioned based changes between the
                // source and destination layouts.
                if ((oldLayouts[0].usages  == newLayouts[0].usages) &&
                    (oldLayouts[0].engines == newLayouts[0].engines))
                {
                    // Skip the depth transition
                    palRangeCount--;

                    palRangeIdx++;
                    layoutIdx++;
                }
                else if ((oldLayouts[1].usages  == newLayouts[1].usages) &&
                         (oldLayouts[1].engines == newLayouts[1].engines))
                {
                    // Skip the stencil transition
                    palRangeCount--;
                }
            }
            else if (pImageMemoryBarriers[i].subresourceRange.aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT)
            {
                VK_ASSERT((pImageMemoryBarriers[i].subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) == 0);

                // Always use the second layout for stencil transitions. It is the only valid one for combined depth
                // stencil layouts, and LayoutUsageHelper replicates stencil-only layouts to all aspects.
                layoutIdx++;
            }
        }

        VK_ASSERT(palRangeCount > 0 && palRangeCount <= MaxPalAspectsPerMask);

        const Image** pLocalImageTransition = pTransitionImages;

        Pal::BarrierTransition* pDestTransition = pNextMain;

        pNextMain += palRangeCount;

        if (pTransitionImages != nullptr)
        {
            const size_t localOffset = (pDestTransition - pTransitions);

            for (uint32_t rangeIdx = 0; rangeIdx < palRangeCount; rangeIdx++)
            {
                pLocalImageTransition[localOffset + rangeIdx] = pImage;
            }
        }

        if (layoutChanging)
        {
            EXTRACT_VK_STRUCTURES_1(
                Barrier,
                ImageMemoryBarrier,
                SampleLocationsInfoEXT,
                &pImageMemoryBarriers[i],
                IMAGE_MEMORY_BARRIER,
                SAMPLE_LOCATIONS_INFO_EXT)

            for (uint32_t transitionIdx = 0; transitionIdx < palRangeCount; transitionIdx++)
            {
                pDestTransition[transitionIdx].srcCacheMask          = barrierTransition.srcCacheMask;
                pDestTransition[transitionIdx].dstCacheMask          = barrierTransition.dstCacheMask;
                pDestTransition[transitionIdx].imageInfo.pImage      = pImage->PalImage(DefaultDeviceIndex);
                pDestTransition[transitionIdx].imageInfo.subresRange = palRanges[palRangeIdx];
                pDestTransition[transitionIdx].imageInfo.oldLayout   = oldLayouts[layoutIdx];
                pDestTransition[transitionIdx].imageInfo.newLayout   = newLayouts[layoutIdx];

                if (pSampleLocationsInfoEXT == nullptr)
                {
                       pDestTransition[transitionIdx].imageInfo.pQuadSamplePattern = 0;
                }
                else if (pLocations != nullptr)  // Could be null due to an OOM error
                {
                    VK_ASSERT(static_cast<uint32_t>(pSampleLocationsInfoEXT->sType) == VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT);
                    VK_ASSERT(pImage->IsSampleLocationsCompatibleDepth());

                    ConvertToPalMsaaQuadSamplePattern(pSampleLocationsInfoEXT, &pLocations[locationIndex]);

                    pDestTransition[transitionIdx].imageInfo.pQuadSamplePattern = &pLocations[locationIndex];
                }

                layoutIdx++;
                palRangeIdx++;
            }

            if (pSampleLocationsInfoEXT != nullptr)
            {
                ++locationIndex;
            }
        }
        else
        {
            for (uint32_t transitionIdx = 0; transitionIdx < palRangeCount; transitionIdx++)
            {
                pDestTransition[transitionIdx].srcCacheMask     = barrierTransition.srcCacheMask;
                pDestTransition[transitionIdx].dstCacheMask     = barrierTransition.dstCacheMask;
                pDestTransition[transitionIdx].imageInfo.pImage = nullptr;
            }
        }

        const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

        // Accounting for the maximum sub ranges, do we have enough space left for another image ?
        const bool full = ((MaxPalAspectsPerMask + mainTransitionCount) > MaxTransitionCount) ||
                          (locationIndex == locationCount);

        if (full)
        {
            FlushBarriers(pBarrier, pTransitions, pTransitionImages, mainTransitionCount);

            pNextMain = pTransitions;
            locationIndex = 0;
        }
    }

    const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

    FlushBarriers(pBarrier, pTransitions, pTransitionImages, mainTransitionCount);

    pVirtStackFrame->FreeArray(pLocations);

    if (pTransitionImages != nullptr)
    {
        pVirtStackFrame->FreeArray(pTransitionImages);
    }

    pVirtStackFrame->FreeArray(pTransitions);
}

// =====================================================================================================================
// Implementation of vkCmdWaitEvents()
void CmdBuffer::WaitEvents(
    uint32_t                     eventCount,
    const VkEvent*               pEvents,
    PipelineStageFlags           srcStageMask,
    PipelineStageFlags           dstStageMask,
    uint32_t                     memoryBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    DbgBarrierPreCmd(DbgBarrierPipelineBarrierWaitEvents);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Allocate space to store signaled event pointers (automatically rewound on unscope)
    const Pal::IGpuEvent** ppGpuEvents = virtStackFrame.AllocArray<const Pal::IGpuEvent*>(NumDeviceEvents(eventCount));

    if (ppGpuEvents != nullptr)
    {
        const uint32_t multiDeviceStride = eventCount;

        for (uint32_t i = 0; i < eventCount; ++i)
        {
            const Event* pEvent = Event::ObjectFromHandle(pEvents[i]);

            InsertDeviceEvents(ppGpuEvents, pEvent, i, multiDeviceStride);
        }

        Pal::BarrierInfo barrier = {};

        // Tell PAL to wait at a specific point until the given set of GpuEvent objects is signaled.
        // We intentionally ignore the source stage flags (srcStagemask) as they are irrelevant in the
        // presence of event objects

        barrier.reason                = RgpBarrierExternalCmdWaitEvents;
        barrier.waitPoint             = VkToPalWaitPipePoint(dstStageMask);
        barrier.gpuEventWaitCount     = eventCount;
        barrier.ppGpuEvents           = ppGpuEvents;

        ExecuteBarriers(&virtStackFrame,
                        memoryBarrierCount,
                        pMemoryBarriers,
                        bufferMemoryBarrierCount,
                        pBufferMemoryBarriers,
                        imageMemoryBarrierCount,
                        pImageMemoryBarriers,
                        &barrier);

        virtStackFrame.FreeArray(ppGpuEvents);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    DbgBarrierPostCmd(DbgBarrierPipelineBarrierWaitEvents);
}

// =====================================================================================================================
// Implementation of vkCmdWaitEvents2()
void CmdBuffer::WaitEvents2(
    uint32_t                   eventCount,
    const VkEvent*             pEvents,
    const VkDependencyInfoKHR* pDependencyInfos)
{
    DbgBarrierPreCmd(DbgBarrierPipelineBarrierWaitEvents);

    // If the ASIC provides split CmdRelease()/CmdReleaseEvent() and CmdAcquire()/CmdAcquireEvent() to express barrier,
    // we will find range of gpu-only events and gpu events with cpu-access, we are assuming the case won't be to have
    // a mixture, it means we can find ranges in the event list that are sync token or not sync token, and then call
    // CmdAcquire() or CmdAcquireEvent() for each range. If the ASIC doesn't support it, we call
    // WaitEventsSync2ToSync1() for all events.
    if (m_flags.useSplitReleaseAcquire)
    {
        uint32_t eventRangeCount = 0;

        for (uint32_t i = 0; i < eventCount; i += eventRangeCount)
        {
            eventRangeCount = 1;

            if (Event::ObjectFromHandle(pEvents[i])->IsUseToken())
            {
                for (uint32_t j = i + 1; j < eventCount; j++)
                {
                    if (Event::ObjectFromHandle(pEvents[j])->IsUseToken())
                    {
                        eventRangeCount++;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                for (uint32_t j = i + 1; j < eventCount; j++)
                {
                    if (Event::ObjectFromHandle(pEvents[j])->IsUseToken())
                    {
                        break;
                    }
                    else
                    {
                        eventRangeCount++;
                    }
                }
            }

            ExecuteAcquireRelease(eventRangeCount,
                                  pEvents + i,
                                  eventRangeCount,
                                  pDependencyInfos + i,
                                  Acquire,
                                  RgpBarrierExternalCmdWaitEvents);
        }
    }
    else
    {
        WaitEventsSync2ToSync1(eventCount,
                               pEvents,
                               eventCount,
                               pDependencyInfos);
    }

    DbgBarrierPostCmd(DbgBarrierPipelineBarrierWaitEvents);
}

// =====================================================================================================================
// Implementation of WaitEvents2()
void CmdBuffer::WaitEventsSync2ToSync1(
    uint32_t                   eventCount,
    const VkEvent*             pEvents,
    uint32_t                   dependencyCount,
    const VkDependencyInfoKHR* pDependencyInfos)
{
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // Allocate space to store signaled event pointers (automatically rewound on unscope)
    const Pal::IGpuEvent** ppGpuEvents = virtStackFrame.AllocArray<const Pal::IGpuEvent*>(NumDeviceEvents(eventCount));

    if (ppGpuEvents != nullptr)
    {
        const uint32_t multiDeviceStride = eventCount;

        for (uint32_t i = 0; i < eventCount; ++i)
        {
            const Event* pEvent = Event::ObjectFromHandle(pEvents[i]);

            InsertDeviceEvents(ppGpuEvents, pEvent, i, multiDeviceStride);
        }

        for (uint32_t j = 0; j < dependencyCount; j++)
        {
            const VkDependencyInfoKHR* pThisDependencyInfo = &pDependencyInfos[j];

            // convert structure VkDependencyInfoKHR to the formal parameters of WaitEvents

            PipelineStageFlags dstStageMask = 0;

            VkMemoryBarrier* pMemoryBarriers = pThisDependencyInfo->memoryBarrierCount > 0 ?
                virtStackFrame.AllocArray<VkMemoryBarrier>(pThisDependencyInfo->memoryBarrierCount) : nullptr;

            for (uint32_t i = 0; i < pThisDependencyInfo->memoryBarrierCount; i++)
            {
                dstStageMask |= pThisDependencyInfo->pMemoryBarriers[i].dstStageMask;

                pMemoryBarriers[i] =
                {
                    VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    pThisDependencyInfo->pMemoryBarriers[i].pNext,
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pMemoryBarriers[i].srcAccessMask),
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pMemoryBarriers[i].dstAccessMask)
                };
            }

            VkBufferMemoryBarrier* pBufferMemoryBarriers = pThisDependencyInfo->bufferMemoryBarrierCount > 0 ?
                virtStackFrame.AllocArray<VkBufferMemoryBarrier>(pThisDependencyInfo->bufferMemoryBarrierCount) :
                nullptr;

            for (uint32_t i = 0; i < pThisDependencyInfo->bufferMemoryBarrierCount; i++)
            {
                dstStageMask |= pThisDependencyInfo->pBufferMemoryBarriers[i].dstStageMask;

                pBufferMemoryBarriers[i] =
                {
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    pThisDependencyInfo->pBufferMemoryBarriers[i].pNext,
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pBufferMemoryBarriers[i].srcAccessMask),
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pBufferMemoryBarriers[i].dstAccessMask),
                    pThisDependencyInfo->pBufferMemoryBarriers[i].srcQueueFamilyIndex,
                    pThisDependencyInfo->pBufferMemoryBarriers[i].dstQueueFamilyIndex,
                    pThisDependencyInfo->pBufferMemoryBarriers[i].buffer,
                    pThisDependencyInfo->pBufferMemoryBarriers[i].offset,
                    pThisDependencyInfo->pBufferMemoryBarriers[i].size
                };
            }

            VkImageMemoryBarrier* pImageMemoryBarriers = pThisDependencyInfo->imageMemoryBarrierCount > 0 ?
                virtStackFrame.AllocArray<VkImageMemoryBarrier>(pThisDependencyInfo->imageMemoryBarrierCount) :
                nullptr;

            for (uint32_t i = 0; i < pThisDependencyInfo->imageMemoryBarrierCount; i++)
            {
                dstStageMask |= pThisDependencyInfo->pImageMemoryBarriers[i].dstStageMask;

                pImageMemoryBarriers[i] =
                {
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    pThisDependencyInfo->pImageMemoryBarriers[i].pNext,
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pImageMemoryBarriers[i].srcAccessMask),
                    static_cast<VkAccessFlags>(pThisDependencyInfo->pImageMemoryBarriers[i].dstAccessMask),
                    pThisDependencyInfo->pImageMemoryBarriers[i].oldLayout,
                    pThisDependencyInfo->pImageMemoryBarriers[i].newLayout,
                    pThisDependencyInfo->pImageMemoryBarriers[i].srcQueueFamilyIndex,
                    pThisDependencyInfo->pImageMemoryBarriers[i].dstQueueFamilyIndex,
                    pThisDependencyInfo->pImageMemoryBarriers[i].image,
                    pThisDependencyInfo->pImageMemoryBarriers[i].subresourceRange
                };
            }

            Pal::BarrierInfo barrier = {};

            barrier.reason                = RgpBarrierExternalCmdWaitEvents;
            barrier.waitPoint             = VkToPalWaitPipePoint(dstStageMask);
            barrier.gpuEventWaitCount     = eventCount;
            barrier.ppGpuEvents           = ppGpuEvents;

            ExecuteBarriers(&virtStackFrame,
                            pThisDependencyInfo->memoryBarrierCount,
                            pMemoryBarriers,
                            pThisDependencyInfo->bufferMemoryBarrierCount,
                            pBufferMemoryBarriers,
                            pThisDependencyInfo->imageMemoryBarrierCount,
                            pImageMemoryBarriers,
                            &barrier);

            if (pMemoryBarriers != nullptr)
            {
                virtStackFrame.FreeArray(pMemoryBarriers);
            }

            if (pBufferMemoryBarriers != nullptr)
            {
                virtStackFrame.FreeArray(pBufferMemoryBarriers);
            }

            if (pImageMemoryBarriers != nullptr)
            {
                virtStackFrame.FreeArray(pImageMemoryBarriers);
            }
        }

        virtStackFrame.FreeArray(ppGpuEvents);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
}

// =====================================================================================================================
// Based on Dependency Info, execute Acquire or Release according to the mode.
void CmdBuffer::ExecuteAcquireRelease(
    uint32_t                   eventCount,
    const VkEvent*             pEvents,
    uint32_t                   dependencyCount,
    const VkDependencyInfoKHR* pDependencyInfos,
    AcquireReleaseMode         acquireReleaseMode,
    uint32_t                   rgpBarrierReasonType)
{
    const RuntimeSettings& settings  = m_pDevice->GetRuntimeSettings();

    uint32_t barrierCount            = 0;
    uint32_t maxBufferMemoryBarriers = 0;
    uint32_t maxImageMemoryBarriers  = 0;

    for (uint32_t i = 0; i < dependencyCount; i++)
    {
        barrierCount += pDependencyInfos[i].memoryBarrierCount + pDependencyInfos[i].bufferMemoryBarrierCount +
                        pDependencyInfos[i].imageMemoryBarrierCount;

        // Determine the maximum number of buffer and image barriers among all the dependency infos passed in
        maxBufferMemoryBarriers = Util::Max(pDependencyInfos[i].bufferMemoryBarrierCount, maxBufferMemoryBarriers);
        maxImageMemoryBarriers  = Util::Max(pDependencyInfos[i].imageMemoryBarrierCount, maxImageMemoryBarriers);
    }

    if ((eventCount > 0) || (barrierCount > 0))
    {
        VirtualStackFrame virtStackFrame(m_pStackAllocator);

        constexpr uint32_t MaxTransitionCount     = 512;
        constexpr uint32_t MaxSampleLocationCount = 128;

        // Keeps track of the number of barriers for which info has already been
        // stored in Pal::AcquireReleaseInfo
        uint32_t memoryBarrierIdx       = 0;
        uint32_t bufferMemoryBarrierIdx = 0;
        uint32_t imageMemoryBarrierIdx  = 0;

        uint32_t maxLocationCount       = Util::Min(maxImageMemoryBarriers, MaxSampleLocationCount);
        uint32_t maxBufferBarrierCount  = Util::Min(maxBufferMemoryBarriers, MaxTransitionCount);
        uint32_t maxImageBarrierCount   = Util::Min((MaxPalAspectsPerMask * maxImageMemoryBarriers) + 1,
                                                    MaxTransitionCount);

        Pal::MemBarrier* pPalBufferMemoryBarriers = (maxBufferMemoryBarriers > 0) ?
            virtStackFrame.AllocArray<Pal::MemBarrier>(maxBufferBarrierCount) : nullptr;

        const Buffer** ppBuffers = (maxBufferMemoryBarriers > 0) ?
            virtStackFrame.AllocArray<const Buffer*>(maxBufferBarrierCount) : nullptr;

        Pal::ImgBarrier* pPalImageBarriers = (maxImageMemoryBarriers > 0) ?
            virtStackFrame.AllocArray<Pal::ImgBarrier>(maxImageBarrierCount) : nullptr;

        const Image** ppImages = (maxImageMemoryBarriers > 0) ?
            virtStackFrame.AllocArray<const Image*>(maxImageBarrierCount) : nullptr;

        Pal::MsaaQuadSamplePattern* pLocations = (maxImageMemoryBarriers > 0) ?
            virtStackFrame.AllocArray<Pal::MsaaQuadSamplePattern>(maxLocationCount) : nullptr;

        const bool bufferAllocSuccess = (((maxBufferMemoryBarriers > 0)         &&
                                          (pPalBufferMemoryBarriers != nullptr) &&
                                          (ppBuffers != nullptr))               ||
                                         (maxBufferMemoryBarriers == 0));

        const bool imageAllocSuccess  = (((maxImageMemoryBarriers > 0)   &&
                                          (pPalImageBarriers != nullptr) &&
                                          (ppImages != nullptr)          &&
                                          (pLocations != nullptr))       ||
                                         (maxImageMemoryBarriers == 0));

        if (bufferAllocSuccess && imageAllocSuccess)
        {
            for (uint32_t j = 0; j < dependencyCount; j++)
            {
                const VkDependencyInfoKHR* pThisDependencyInfo = &pDependencyInfos[j];

                uint32_t memBarrierCount          = pThisDependencyInfo->memoryBarrierCount;
                uint32_t bufferMemoryBarrierCount = pThisDependencyInfo->bufferMemoryBarrierCount;
                uint32_t imageMemoryBarrierCount  = pThisDependencyInfo->imageMemoryBarrierCount;

                while ((memoryBarrierIdx < memBarrierCount) ||
                       (bufferMemoryBarrierIdx < bufferMemoryBarrierCount) ||
                       (imageMemoryBarrierIdx < imageMemoryBarrierCount))
                {
                    Pal::AcquireReleaseInfo acquireReleaseInfo = {};

                    acquireReleaseInfo.pMemoryBarriers = pPalBufferMemoryBarriers;
                    acquireReleaseInfo.pImageBarriers  = pPalImageBarriers;
                    acquireReleaseInfo.reason          = rgpBarrierReasonType;

                    uint32_t locationIndex = 0;

                    while (memoryBarrierIdx < memBarrierCount)
                    {
                        Pal::BarrierTransition tempTransition = {};

                        const VkMemoryBarrier2& memoryBarrier = pThisDependencyInfo->pMemoryBarriers[memoryBarrierIdx];

                        acquireReleaseInfo.srcGlobalStageMask |= VkToPalPipelineStageFlags(memoryBarrier.srcStageMask,
                                                                                           true);
                        acquireReleaseInfo.dstGlobalStageMask |= VkToPalPipelineStageFlags(memoryBarrier.dstStageMask,
                                                                                           false);

                        VkAccessFlags2KHR srcAccessMask = memoryBarrier.srcAccessMask;
                        VkAccessFlags2KHR dstAccessMask = memoryBarrier.dstAccessMask;

                        m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
                            srcAccessMask,
                            dstAccessMask,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_GENERAL,
                            &tempTransition);

                        acquireReleaseInfo.srcGlobalAccessMask |= tempTransition.srcCacheMask;
                        acquireReleaseInfo.dstGlobalAccessMask |= tempTransition.dstCacheMask;

                        memoryBarrierIdx++;
                    }

                    while ((acquireReleaseInfo.memoryBarrierCount < maxBufferBarrierCount) &&
                           (bufferMemoryBarrierIdx < bufferMemoryBarrierCount))
                    {
                        Pal::BarrierTransition tempTransition = {};

                        const VkBufferMemoryBarrier2& bufferMemoryBarrier =
                            pThisDependencyInfo->pBufferMemoryBarriers[bufferMemoryBarrierIdx];

                        const Buffer* pBuffer = Buffer::ObjectFromHandle(bufferMemoryBarrier.buffer);

                        pBuffer->GetBarrierPolicy().ApplyBufferMemoryBarrier<VkBufferMemoryBarrier2KHR>(
                            GetQueueFamilyIndex(),
                            bufferMemoryBarrier,
                            &tempTransition);

                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].srcStageMask   =
                            VkToPalPipelineStageFlags(bufferMemoryBarrier.srcStageMask, true);
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].dstStageMask   =
                            VkToPalPipelineStageFlags(bufferMemoryBarrier.dstStageMask, false);
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].flags.u32All   =
                            0;
                        // We set the address to 0 by default here. But, this will be computed correctly later for each
                        // device including DefaultDeviceIndex based on the deviceId.
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.address =
                            0;
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.offset  =
                            bufferMemoryBarrier.offset;
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.size    =
                            bufferMemoryBarrier.size;
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].srcAccessMask  =
                            tempTransition.srcCacheMask;
                        pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].dstAccessMask  =
                            tempTransition.dstCacheMask;

                        ppBuffers[acquireReleaseInfo.memoryBarrierCount] = pBuffer;

                        acquireReleaseInfo.memoryBarrierCount++;

                        bufferMemoryBarrierIdx++;
                    }

                    // Accounting for the max sub ranges, if we do not have enough space left for another image,
                    // break from this loop. The info for remaining barriers will be passed to PAL in subsequent calls.
                    while (((MaxPalAspectsPerMask + acquireReleaseInfo.imageBarrierCount) < maxImageBarrierCount) &&
                            (locationIndex < maxLocationCount) &&
                            (imageMemoryBarrierIdx < imageMemoryBarrierCount))
                    {
                        Pal::BarrierTransition tempTransition = {};

                        const VkImageMemoryBarrier2& imageMemoryBarrier =
                            pThisDependencyInfo->pImageMemoryBarriers[imageMemoryBarrierIdx];

                        bool layoutChanging = false;
                        Pal::ImageLayout oldLayouts[MaxPalAspectsPerMask];
                        Pal::ImageLayout newLayouts[MaxPalAspectsPerMask];

                        const Image* pImage = Image::ObjectFromHandle(imageMemoryBarrier.image);

                        // Synchronization2 will use new PAL interfaces CmdAcquire(), CmdRelease() and
                        // CmdReleaseThenAcquire() to execute barrier, Under these interfaces, vulkan driver does not
                        // need to add an optimization for Image barrier with the same oldLayout & newLayout, like
                        // VK_IMAGE_LAYOUT_GENERAL to VK_IMAGE_LAYOUT_GENERAL. PAL should not be doing any transition
                        // logic and only flush/invalidate caches as apporiate. So we make use of the template flag
                        // skipMatchingLayouts to skip this if-checking for the same layout change by setting the flag
                        // skipMatchingLayouts to false. As for legacy synchronization, we should be careful of this
                        // change, maybe will have some potential regressions, so currently we keep this optimization
                        // unchanged by setting this flag to true. With the iterative update of vulkan driver, we should
                        // also remove this optimization for legacy synchronization.
                        pImage->GetBarrierPolicy().ApplyImageMemoryBarrier<VkImageMemoryBarrier2KHR>(
                            GetQueueFamilyIndex(),
                            imageMemoryBarrier,
                            &tempTransition,
                            &layoutChanging,
                            oldLayouts,
                            newLayouts,
                            false);

                        VkFormat format = pImage->GetFormat();

                        uint32_t layoutIdx     = 0;
                        uint32_t palRangeIdx   = 0;
                        uint32_t palRangeCount = 0;

                        Pal::SubresRange palRanges[MaxPalAspectsPerMask];

                        VkToPalSubresRange(
                            format,
                            imageMemoryBarrier.subresourceRange,
                            pImage->GetMipLevels(),
                            pImage->GetArraySize(),
                            palRanges,
                            &palRangeCount,
                            settings);

                        if (Formats::HasStencil(format))
                        {
                            const VkImageAspectFlags aspectMask = imageMemoryBarrier.subresourceRange.aspectMask;

                            // Always use the second layout for stencil transitions. It is the only valid one for
                            // combined depth stencil layouts, and LayoutUsageHelper replicates stencil-only layouts to
                            // all aspects.
                            if ((aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                                ((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) == 0))
                            {
                                layoutIdx++;
                            }
                        }

                        EXTRACT_VK_STRUCTURES_1(
                            Barrier,
                            ImageMemoryBarrier2KHR,
                            SampleLocationsInfoEXT,
                            &imageMemoryBarrier,
                            IMAGE_MEMORY_BARRIER_2_KHR,
                            SAMPLE_LOCATIONS_INFO_EXT)

                        for (uint32_t transitionIdx = 0; transitionIdx < palRangeCount; transitionIdx++)
                        {
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].srcStageMask       =
                                VkToPalPipelineStageFlags(imageMemoryBarrier.srcStageMask, true);
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].dstStageMask       =
                                VkToPalPipelineStageFlags(imageMemoryBarrier.dstStageMask, false);
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].srcAccessMask      =
                                tempTransition.srcCacheMask;
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].dstAccessMask      =
                                tempTransition.dstCacheMask;
                            // We set the pImage to nullptr by default here. But, this will be computed correctly later
                            // for each device including DefaultDeviceIndex based on the deviceId.
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pImage             =
                                nullptr;
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].subresRange        =
                                palRanges[palRangeIdx];
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].oldLayout          =
                                oldLayouts[layoutIdx];
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].newLayout          =
                                newLayouts[layoutIdx];
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern =
                                nullptr;

                            ppImages[acquireReleaseInfo.imageBarrierCount] = pImage;

                            if (pSampleLocationsInfoEXT == nullptr)
                            {
                                pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern = nullptr;
                            }
                            else if (pLocations != nullptr)  // Could be null due to an OOM error
                            {
                                VK_ASSERT(static_cast<uint32_t>(pSampleLocationsInfoEXT->sType) ==
                                            VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT);
                                VK_ASSERT(pImage->IsSampleLocationsCompatibleDepth());

                                ConvertToPalMsaaQuadSamplePattern(pSampleLocationsInfoEXT, &pLocations[locationIndex]);

                                pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern =
                                    &pLocations[locationIndex];
                            }

                            acquireReleaseInfo.imageBarrierCount++;

                            layoutIdx++;
                            palRangeIdx++;
                        }

                        if (pSampleLocationsInfoEXT != nullptr)
                        {
                            ++locationIndex;
                        }

                        imageMemoryBarrierIdx++;
                    }

                    if (acquireReleaseMode == Release)
                    {
                        acquireReleaseInfo.dstGlobalStageMask  = 0;
                        acquireReleaseInfo.dstGlobalAccessMask = 0;

                        // If memoryBarrierCount is 0, set srcStageMask to Pal::PipelineStageTopOfPipe.
                        if (acquireReleaseInfo.srcGlobalStageMask == 0)
                        {
                            acquireReleaseInfo.srcGlobalStageMask |= Pal::PipelineStageTopOfPipe;
                        }

                        for (uint32 i = 0; i < acquireReleaseInfo.memoryBarrierCount; i++)
                        {
                            pPalBufferMemoryBarriers[i].dstStageMask  = 0;
                            pPalBufferMemoryBarriers[i].dstAccessMask = 0;
                        }

                        for (uint32 i = 0; i < acquireReleaseInfo.imageBarrierCount; i++)
                        {
                            pPalImageBarriers[i].dstStageMask  = 0;
                            pPalImageBarriers[i].dstAccessMask = 0;
                        }

                        PalCmdRelease(
                            &acquireReleaseInfo,
                            eventCount,
                            pEvents,
                            pPalBufferMemoryBarriers,
                            ppBuffers,
                            pPalImageBarriers,
                            ppImages,
                            m_curDeviceMask);
                    }
                    else if (acquireReleaseMode == Acquire)
                    {
                        acquireReleaseInfo.srcGlobalStageMask  = 0;
                        acquireReleaseInfo.srcGlobalAccessMask = 0;

                        for (uint32 i = 0; i < acquireReleaseInfo.memoryBarrierCount; i++)
                        {
                            pPalBufferMemoryBarriers[i].srcStageMask  = 0;
                            pPalBufferMemoryBarriers[i].srcAccessMask = 0;
                        }

                        for (uint32 i = 0; i < acquireReleaseInfo.imageBarrierCount; i++)
                        {
                            pPalImageBarriers[i].srcStageMask  = 0;
                            pPalImageBarriers[i].srcAccessMask = 0;
                        }

                        PalCmdAcquire(
                            &acquireReleaseInfo,
                            eventCount,
                            pEvents,
                            pPalBufferMemoryBarriers,
                            ppBuffers,
                            pPalImageBarriers,
                            ppImages,
                            &virtStackFrame,
                            m_curDeviceMask);
                    }
                    else
                    {
                        PalCmdReleaseThenAcquire(
                            &acquireReleaseInfo,
                            pPalBufferMemoryBarriers,
                            ppBuffers,
                            pPalImageBarriers,
                            ppImages,
                            m_curDeviceMask);
                    }
                }
            }
        }
        else
        {
            m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        if (pPalBufferMemoryBarriers != nullptr)
        {
            virtStackFrame.FreeArray<Pal::MemBarrier>(pPalBufferMemoryBarriers);
        }

        if (ppBuffers != nullptr)
        {
            virtStackFrame.FreeArray<const Buffer*>(ppBuffers);
        }

        if (pPalImageBarriers != nullptr)
        {
            virtStackFrame.FreeArray<Pal::ImgBarrier>(pPalImageBarriers);
        }

        if (ppImages != nullptr)
        {
            virtStackFrame.FreeArray<const Image*>(ppImages);
        }

        if (pLocations != nullptr)
        {
            virtStackFrame.FreeArray<Pal::MsaaQuadSamplePattern>(pLocations);
        }
    }
}

// =====================================================================================================================
// Execute Release then acquire mode
void CmdBuffer::ExecuteReleaseThenAcquire(
    PipelineStageFlags           srcStageMask,
    PipelineStageFlags           dstStageMask,
    uint32_t                     memBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    if ((memBarrierCount + bufferMemoryBarrierCount + imageMemoryBarrierCount) > 0)
    {
        VirtualStackFrame virtStackFrame(m_pStackAllocator);

        const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

        constexpr uint32_t MaxTransitionCount     = 512;
        constexpr uint32_t MaxSampleLocationCount = 128;

        // Keeps track of the number of barriers for which info has already been
        // stored in Pal::AcquireReleaseInfo
        uint32_t memoryBarrierIdx       = 0;
        uint32_t bufferMemoryBarrierIdx = 0;
        uint32_t imageMemoryBarrierIdx  = 0;

        uint32_t maxLocationCount       = Util::Min(imageMemoryBarrierCount, MaxSampleLocationCount);
        uint32_t maxBufferBarrierCount  = Util::Min(bufferMemoryBarrierCount, MaxTransitionCount);
        uint32_t maxImageBarrierCount   = Util::Min((MaxPalAspectsPerMask * imageMemoryBarrierCount) + 1,
                                                    MaxTransitionCount);

        Pal::MemBarrier* pPalBufferMemoryBarriers = (bufferMemoryBarrierCount > 0) ?
            virtStackFrame.AllocArray<Pal::MemBarrier>(maxBufferBarrierCount) : nullptr;

        const Buffer** ppBuffers = (bufferMemoryBarrierCount > 0) ?
            virtStackFrame.AllocArray<const Buffer*>(maxBufferBarrierCount) : nullptr;

        Pal::ImgBarrier* pPalImageBarriers = (imageMemoryBarrierCount > 0) ?
            virtStackFrame.AllocArray<Pal::ImgBarrier>(maxImageBarrierCount) : nullptr;

        Pal::MsaaQuadSamplePattern* pLocations = (imageMemoryBarrierCount > 0) ?
            virtStackFrame.AllocArray<Pal::MsaaQuadSamplePattern>(maxLocationCount) : nullptr;

        const Image** ppImages = (imageMemoryBarrierCount > 0) ?
            virtStackFrame.AllocArray<const Image*>(maxImageBarrierCount) : nullptr;

        const bool bufferAllocSuccess = (((bufferMemoryBarrierCount > 0)        &&
                                          (pPalBufferMemoryBarriers != nullptr) &&
                                          (ppBuffers != nullptr))               ||
                                         (bufferMemoryBarrierCount == 0));

        const bool imageAllocSuccess  = (((imageMemoryBarrierCount > 0)  &&
                                          (pPalImageBarriers != nullptr) &&
                                          (ppImages != nullptr)          &&
                                          (pLocations != nullptr))       ||
                                         (imageMemoryBarrierCount == 0));

        if (bufferAllocSuccess && imageAllocSuccess)
        {
            while ((memoryBarrierIdx < memBarrierCount) ||
                   (bufferMemoryBarrierIdx < bufferMemoryBarrierCount) ||
                   (imageMemoryBarrierIdx < imageMemoryBarrierCount))
            {
                Pal::AcquireReleaseInfo acquireReleaseInfo = {};

                acquireReleaseInfo.pMemoryBarriers = pPalBufferMemoryBarriers;
                acquireReleaseInfo.pImageBarriers  = pPalImageBarriers;
                acquireReleaseInfo.reason          = RgpBarrierExternalCmdPipelineBarrier;

                uint32_t palSrcStageMask = VkToPalPipelineStageFlags(srcStageMask, true);
                uint32_t palDstStageMask = VkToPalPipelineStageFlags(dstStageMask, false);

                uint32_t locationIndex = 0;

                while (memoryBarrierIdx < memBarrierCount)
                {
                    Pal::BarrierTransition tempTransition = {};

                    VkAccessFlags srcAccessMask = pMemoryBarriers[memoryBarrierIdx].srcAccessMask;
                    VkAccessFlags dstAccessMask = pMemoryBarriers[memoryBarrierIdx].dstAccessMask;

                    m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
                        srcAccessMask,
                        dstAccessMask,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_IMAGE_LAYOUT_GENERAL,
                        &tempTransition);

                    acquireReleaseInfo.srcGlobalStageMask   = palSrcStageMask;
                    acquireReleaseInfo.dstGlobalStageMask   = palDstStageMask;
                    acquireReleaseInfo.srcGlobalAccessMask |= tempTransition.srcCacheMask;
                    acquireReleaseInfo.dstGlobalAccessMask |= tempTransition.dstCacheMask;

                    memoryBarrierIdx++;
                }

                while ((acquireReleaseInfo.memoryBarrierCount < maxBufferBarrierCount) &&
                       (bufferMemoryBarrierIdx < bufferMemoryBarrierCount))
                {
                    Pal::BarrierTransition tempTransition = {};

                    const Buffer* pBuffer = Buffer::ObjectFromHandle(
                        pBufferMemoryBarriers[bufferMemoryBarrierIdx].buffer);

                    pBuffer->GetBarrierPolicy().ApplyBufferMemoryBarrier<VkBufferMemoryBarrier>(
                        GetQueueFamilyIndex(),
                        pBufferMemoryBarriers[bufferMemoryBarrierIdx],
                        &tempTransition);

                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].flags.u32All      =
                        0;
                    // We set the address to 0 by default here. But, this will be computed correctly later for each
                    // device including DefaultDeviceIndex based on the deviceId
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.address    =
                        0;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.offset     =
                        pBufferMemoryBarriers[bufferMemoryBarrierIdx].offset;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].memory.size       =
                        pBufferMemoryBarriers[bufferMemoryBarrierIdx].size;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].srcStageMask      =
                        palSrcStageMask;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].dstStageMask      =
                        palDstStageMask;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].srcAccessMask     =
                        tempTransition.srcCacheMask;
                    pPalBufferMemoryBarriers[acquireReleaseInfo.memoryBarrierCount].dstAccessMask     =
                        tempTransition.dstCacheMask;

                    ppBuffers[acquireReleaseInfo.memoryBarrierCount] = pBuffer;

                    acquireReleaseInfo.memoryBarrierCount++;

                    bufferMemoryBarrierIdx++;
                }

                // Accounting for the max sub ranges, if we do not have enough space left for another image,
                // break from this loop. The info for remaining barriers will be passed to PAL in subsequent calls.
                while (((MaxPalAspectsPerMask + acquireReleaseInfo.imageBarrierCount) < maxImageBarrierCount) &&
                       (locationIndex < maxLocationCount) &&
                       (imageMemoryBarrierIdx < imageMemoryBarrierCount))
                {
                    Pal::BarrierTransition tempTransition = {};

                    bool layoutChanging = false;
                    Pal::ImageLayout oldLayouts[MaxPalAspectsPerMask];
                    Pal::ImageLayout newLayouts[MaxPalAspectsPerMask];

                    const Image* pImage = Image::ObjectFromHandle(pImageMemoryBarriers[imageMemoryBarrierIdx].image);

                    // When using CmdReleaseThenAcquire() to execute barriers, vulkan driver does not need to add an
                    // optimization for Image barrier with the same oldLayout & newLayout,like VK_IMAGE_LAYOUT_GENERAL
                    // to VK_IMAGE_LAYOUT_GENERAL. PAL should not be doing any transition logic and only flush or
                    // invalidate caches as apporiate. so we make use of the template flag skipMatchingLayouts to skip
                    // this if-checking for the same layout change by setting the flag skipMatchingLayouts to false.
                    pImage->GetBarrierPolicy().ApplyImageMemoryBarrier<VkImageMemoryBarrier>(
                        GetQueueFamilyIndex(),
                        pImageMemoryBarriers[imageMemoryBarrierIdx],
                        &tempTransition,
                        &layoutChanging,
                        oldLayouts,
                        newLayouts,
                        false);

                    VkFormat format = pImage->GetFormat();

                    uint32_t layoutIdx     = 0;
                    uint32_t palRangeIdx   = 0;
                    uint32_t palRangeCount = 0;

                    Pal::SubresRange palRanges[MaxPalAspectsPerMask];

                    VkToPalSubresRange(
                        format,
                        pImageMemoryBarriers[imageMemoryBarrierIdx].subresourceRange,
                        pImage->GetMipLevels(),
                        pImage->GetArraySize(),
                        palRanges,
                        &palRangeCount,
                        settings);

                    if (Formats::HasStencil(format))
                    {
                        const VkImageAspectFlags aspectMask =
                            pImageMemoryBarriers[imageMemoryBarrierIdx].subresourceRange.aspectMask;

                        // Always use the second layout for stencil transitions. It is the only valid one for combined
                        // depth stencil layouts, and LayoutUsageHelper replicates stencil-only layouts to all aspects.
                        if ((aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) &&
                            ((aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) == 0))
                        {
                            layoutIdx++;
                        }
                    }

                    EXTRACT_VK_STRUCTURES_1(
                        Barrier,
                        ImageMemoryBarrier,
                        SampleLocationsInfoEXT,
                        &pImageMemoryBarriers[imageMemoryBarrierIdx],
                        IMAGE_MEMORY_BARRIER,
                        SAMPLE_LOCATIONS_INFO_EXT)

                    for (uint32_t transitionIdx = 0; transitionIdx < palRangeCount; transitionIdx++)
                    {
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].srcStageMask       =
                            palSrcStageMask;
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].dstStageMask       =
                            palDstStageMask;
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].srcAccessMask      =
                            tempTransition.srcCacheMask;
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].dstAccessMask      =
                            tempTransition.dstCacheMask;
                        // We set the pImage to nullptr by default here. But, this will be computed correctly later for
                        // each device including DefaultDeviceIndex based on the deviceId.
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pImage             =
                            nullptr;
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].subresRange        =
                            palRanges[palRangeIdx];
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].oldLayout          =
                            oldLayouts[layoutIdx];
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].newLayout          =
                            newLayouts[layoutIdx];
                        pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern =
                            nullptr;

                        ppImages[acquireReleaseInfo.imageBarrierCount] = pImage;

                        if (pSampleLocationsInfoEXT == nullptr)
                        {
                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern = nullptr;
                        }
                        else if (pLocations != nullptr)  // Could be null due to an OOM error
                        {
                            VK_ASSERT(static_cast<uint32_t>(pSampleLocationsInfoEXT->sType) ==
                                VK_STRUCTURE_TYPE_SAMPLE_LOCATIONS_INFO_EXT);
                            VK_ASSERT(pImage->IsSampleLocationsCompatibleDepth());

                            ConvertToPalMsaaQuadSamplePattern(pSampleLocationsInfoEXT, &pLocations[locationIndex]);

                            pPalImageBarriers[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern =
                                &pLocations[locationIndex];
                        }

                        acquireReleaseInfo.imageBarrierCount++;

                        layoutIdx++;
                        palRangeIdx++;
                    }

                    if (pSampleLocationsInfoEXT != nullptr)
                    {
                        ++locationIndex;
                    }

                    imageMemoryBarrierIdx++;
                }

                PalCmdReleaseThenAcquire(
                    &acquireReleaseInfo,
                    pPalBufferMemoryBarriers,
                    ppBuffers,
                    pPalImageBarriers,
                    ppImages,
                    m_curDeviceMask);
            }
        }
        else
        {
            m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        if (pPalBufferMemoryBarriers != nullptr)
        {
            virtStackFrame.FreeArray<Pal::MemBarrier>(pPalBufferMemoryBarriers);
        }

        if (ppBuffers != nullptr)
        {
            virtStackFrame.FreeArray<const Buffer*>(ppBuffers);
        }

        if (pPalImageBarriers != nullptr)
        {
            virtStackFrame.FreeArray<Pal::ImgBarrier>(pPalImageBarriers);
        }

        if (ppImages != nullptr)
        {
            virtStackFrame.FreeArray<const Image*>(ppImages);
        }

        if (pLocations != nullptr)
        {
            virtStackFrame.FreeArray<Pal::MsaaQuadSamplePattern>(pLocations);
        }
    }
}

// =====================================================================================================================
// Implements of vkCmdPipelineBarrier()
void CmdBuffer::PipelineBarrier(
    PipelineStageFlags           srcStageMask,
    PipelineStageFlags           destStageMask,
    uint32_t                     memBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    DbgBarrierPreCmd(DbgBarrierPipelineBarrierWaitEvents);

    if (m_flags.useReleaseAcquire)
    {
        ExecuteReleaseThenAcquire(srcStageMask,
                                  destStageMask,
                                  memBarrierCount,
                                  pMemoryBarriers,
                                  bufferMemoryBarrierCount,
                                  pBufferMemoryBarriers,
                                  imageMemoryBarrierCount,
                                  pImageMemoryBarriers);
    }
    else
    {
        VirtualStackFrame virtStackFrame(m_pStackAllocator);

        Pal::BarrierInfo barrier = {};

        // Tell PAL to wait at a specific point until the given set of pipeline events has been signaled (this version
        // does not use GpuEvent objects).
        barrier.reason       = RgpBarrierExternalCmdPipelineBarrier;
        barrier.waitPoint    = VkToPalWaitPipePoint(destStageMask);

        // Collect signal pipe points.
        Pal::HwPipePoint pipePoints[MaxHwPipePoints];

        barrier.pipePointWaitCount      = VkToPalSrcPipePoints(srcStageMask, pipePoints);
        barrier.pPipePoints             = pipePoints;

        ExecuteBarriers(&virtStackFrame,
                        memBarrierCount,
                        pMemoryBarriers,
                        bufferMemoryBarrierCount,
                        pBufferMemoryBarriers,
                        imageMemoryBarrierCount,
                        pImageMemoryBarriers,
                        &barrier);

        DbgBarrierPostCmd(DbgBarrierPipelineBarrierWaitEvents);
    }
}

// =====================================================================================================================
// Implements of vkCmdPipelineBarrier2()
void CmdBuffer::PipelineBarrier2(
    const VkDependencyInfoKHR*   pDependencyInfo)
{
    DbgBarrierPreCmd(DbgBarrierPipelineBarrierWaitEvents);

    if (m_flags.useReleaseAcquire)
    {
        ExecuteAcquireRelease(0,
                              nullptr,
                              1,
                              pDependencyInfo,
                              ReleaseThenAcquire,
                              RgpBarrierExternalCmdPipelineBarrier);
    }
    else
    {
        PipelineBarrierSync2ToSync1(pDependencyInfo);
    }

    DbgBarrierPostCmd(DbgBarrierPipelineBarrierWaitEvents);
}

// =====================================================================================================================
// Implements of PipelineBarrier2
void CmdBuffer::PipelineBarrierSync2ToSync1(
    const VkDependencyInfoKHR*   pDependencyInfo)
{
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    // convert structure VkDependencyInfoKHR to the formal parameters of PipelineBarrier
    VK_ASSERT((pDependencyInfo->memoryBarrierCount + pDependencyInfo->bufferMemoryBarrierCount +
        pDependencyInfo->imageMemoryBarrierCount) != 0);

    PipelineStageFlags srcStageMask = 0;
    PipelineStageFlags dstStageMask = 0;

    VkMemoryBarrier* pMemoryBarriers = pDependencyInfo->memoryBarrierCount > 0 ?
        virtStackFrame.AllocArray<VkMemoryBarrier>(pDependencyInfo->memoryBarrierCount) : nullptr;

    for (uint32_t i = 0; i < pDependencyInfo->memoryBarrierCount; i++)
    {
        srcStageMask |= pDependencyInfo->pMemoryBarriers[i].srcStageMask;
        dstStageMask |= pDependencyInfo->pMemoryBarriers[i].dstStageMask;

        pMemoryBarriers[i] =
        {
            VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            pDependencyInfo->pMemoryBarriers[i].pNext,
            static_cast<VkAccessFlags>(pDependencyInfo->pMemoryBarriers[i].srcAccessMask),
            static_cast<VkAccessFlags>(pDependencyInfo->pMemoryBarriers[i].dstAccessMask)
        };
    }

    VkBufferMemoryBarrier* pBufferMemoryBarriers = pDependencyInfo->bufferMemoryBarrierCount > 0 ?
        virtStackFrame.AllocArray<VkBufferMemoryBarrier>(pDependencyInfo->bufferMemoryBarrierCount) : nullptr;

    for (uint32_t i = 0; i < pDependencyInfo->bufferMemoryBarrierCount; i++)
    {
        srcStageMask |= pDependencyInfo->pBufferMemoryBarriers[i].srcStageMask;
        dstStageMask |= pDependencyInfo->pBufferMemoryBarriers[i].dstStageMask;

        pBufferMemoryBarriers[i] =
        {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            pDependencyInfo->pBufferMemoryBarriers[i].pNext,
            static_cast<VkAccessFlags>(pDependencyInfo->pBufferMemoryBarriers[i].srcAccessMask),
            static_cast<VkAccessFlags>(pDependencyInfo->pBufferMemoryBarriers[i].dstAccessMask),
            pDependencyInfo->pBufferMemoryBarriers[i].srcQueueFamilyIndex,
            pDependencyInfo->pBufferMemoryBarriers[i].dstQueueFamilyIndex,
            pDependencyInfo->pBufferMemoryBarriers[i].buffer,
            pDependencyInfo->pBufferMemoryBarriers[i].offset,
            pDependencyInfo->pBufferMemoryBarriers[i].size
        };
    }

    VkImageMemoryBarrier* pImageMemoryBarriers = pDependencyInfo->imageMemoryBarrierCount > 0 ?
        virtStackFrame.AllocArray<VkImageMemoryBarrier>(pDependencyInfo->imageMemoryBarrierCount) : nullptr;

    for (uint32_t i = 0; i < pDependencyInfo->imageMemoryBarrierCount; i++)
    {
        srcStageMask |= pDependencyInfo->pImageMemoryBarriers[i].srcStageMask;
        dstStageMask |= pDependencyInfo->pImageMemoryBarriers[i].dstStageMask;

        pImageMemoryBarriers[i] =
        {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            pDependencyInfo->pImageMemoryBarriers[i].pNext,
            static_cast<VkAccessFlags>(pDependencyInfo->pImageMemoryBarriers[i].srcAccessMask),
            static_cast<VkAccessFlags>(pDependencyInfo->pImageMemoryBarriers[i].dstAccessMask),
            pDependencyInfo->pImageMemoryBarriers[i].oldLayout,
            pDependencyInfo->pImageMemoryBarriers[i].newLayout,
            pDependencyInfo->pImageMemoryBarriers[i].srcQueueFamilyIndex,
            pDependencyInfo->pImageMemoryBarriers[i].dstQueueFamilyIndex,
            pDependencyInfo->pImageMemoryBarriers[i].image,
            pDependencyInfo->pImageMemoryBarriers[i].subresourceRange
        };
    }

    Pal::BarrierInfo barrier = {};

    // Tell PAL to wait at a specific point until the given set of pipeline events has been signaled (this version
    // does not use GpuEvent objects).
    barrier.reason       = RgpBarrierExternalCmdPipelineBarrier;
    barrier.waitPoint    = VkToPalWaitPipePoint(dstStageMask);

    // Collect signal pipe points.
    Pal::HwPipePoint pipePoints[MaxHwPipePoints];

    barrier.pipePointWaitCount      = VkToPalSrcPipePoints(srcStageMask, pipePoints);
    barrier.pPipePoints             = pipePoints;

    ExecuteBarriers(&virtStackFrame,
                    pDependencyInfo->memoryBarrierCount,
                    pMemoryBarriers,
                    pDependencyInfo->bufferMemoryBarrierCount,
                    pBufferMemoryBarriers,
                    pDependencyInfo->imageMemoryBarrierCount,
                    pImageMemoryBarriers,
                    &barrier);

    if (pMemoryBarriers != nullptr)
    {
        virtStackFrame.FreeArray(pMemoryBarriers);
    }

    if (pBufferMemoryBarriers != nullptr)
    {
        virtStackFrame.FreeArray(pBufferMemoryBarriers);
    }

    if (pImageMemoryBarriers != nullptr)
    {
        virtStackFrame.FreeArray(pImageMemoryBarriers);
    }
}

// =====================================================================================================================
void CmdBuffer::BeginQueryIndexed(
    VkQueryPool         queryPool,
    uint32_t            query,
    VkQueryControlFlags flags,
    uint32_t            index)
{
    DbgBarrierPreCmd(DbgBarrierQueryBeginEnd);

    const QueryPool* pBasePool = QueryPool::ObjectFromHandle(queryPool);
    const auto palQueryControlFlags = VkToPalQueryControlFlags(pBasePool->GetQueryType(), flags);

    // NOTE: This function is illegal to call for TimestampQueryPools and AccelerationStructureQueryPools
    const PalQueryPool* pQueryPool = pBasePool->AsPalQueryPool();
    Pal::QueryType queryType = pQueryPool->PalQueryType();
    if (queryType == Pal::QueryType::StreamoutStats)
    {
        queryType = static_cast<Pal::QueryType>(static_cast<uint32_t>(queryType) + index);
    }

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdBeginQuery(*pQueryPool->PalPool(deviceIdx),
            queryType,
            query,
            palQueryControlFlags);
    }
    while (deviceGroup.IterateNext());

    const auto* const pRenderPass = m_allGpuState.pRenderPass;

    // If queries are used while executing a render pass instance that has multiview enabled,
    // the query uses N consecutive query indices in the query pool (starting at query) where
    // N is the number of bits set in the view mask in the subpass the query is used in.
    //
    // Implementations may write the total result to the first query and
    // write zero to the other queries.
    if (((pRenderPass != nullptr) && pRenderPass->IsMultiviewEnabled()) ||
        (m_allGpuState.dynamicRenderingInstance.viewMask != 0))
    {
        const auto viewMask  = (pRenderPass != nullptr) ? pRenderPass->GetViewMask(m_renderPassInstance.subpass) :
            m_allGpuState.dynamicRenderingInstance.viewMask;

        const auto viewCount = Util::CountSetBits(viewMask);

        // Call Begin() and immediately call End() for all remaining queries,
        // to set value of each remaining query to 0 and to make them avaliable.
        for (uint32_t remainingQuery = 1; remainingQuery < viewCount; ++remainingQuery)
        {
            const auto remainingQueryIndex = query + remainingQuery;

            utils::IterateMask multiviewDeviceGroup(m_curDeviceMask);
            do
            {
                const uint32_t deviceIdx = multiviewDeviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdBeginQuery(
                    *pQueryPool->PalPool(deviceIdx),
                    pQueryPool->PalQueryType(),
                    remainingQueryIndex, palQueryControlFlags);

                PalCmdBuffer(deviceIdx)->CmdEndQuery(
                    *pQueryPool->PalPool(deviceIdx),
                    pQueryPool->PalQueryType(),
                    remainingQueryIndex);
            }
            while (multiviewDeviceGroup.IterateNext());
        }
    }

    DbgBarrierPostCmd(DbgBarrierQueryBeginEnd);
}

// =====================================================================================================================
void CmdBuffer::EndQueryIndexed(
    VkQueryPool queryPool,
    uint32_t    query,
    uint32_t    index)
{
    DbgBarrierPreCmd(DbgBarrierQueryBeginEnd);

    // NOTE: This function is illegal to call for TimestampQueryPools and  AccelerationStructureQueryPools
    const PalQueryPool* pQueryPool = QueryPool::ObjectFromHandle(queryPool)->AsPalQueryPool();
    Pal::QueryType queryType = pQueryPool->PalQueryType();
    if (queryType == Pal::QueryType::StreamoutStats)
    {
        queryType = static_cast<Pal::QueryType>(static_cast<uint32_t>(queryType) + index);
    }

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdEndQuery(*pQueryPool->PalPool(deviceIdx),
            queryType,
            query);
    }
    while (deviceGroup.IterateNext());

    DbgBarrierPostCmd(DbgBarrierQueryBeginEnd);
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void CmdBuffer::ResetAccelerationStructureQueryPool(
    const AccelerationStructureQueryPool& accelerationStructureQueryPool,
    const uint32_t                        firstQuery,
    const uint32_t                        queryCount)
{
    // All the cache operations operating on the query pool's accelerationStructure memory
    // that may have occurred before/after this reset.
    static const uint32_t AccelerationStructureCoher =
        Pal::CoherShaderWrite |     // vkWriteAccelerationStructuresProperties (CmdDispatch)
        Pal::CoherShaderRead  |     // vkCmdCopyQueryPoolResults
        Pal::CoherCopyDst;          // vkCmdResetQueryPool (CmdFillMemory)

    static const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;

    // Wait for any accelerationStructure query pool events to complete prior to filling memory
    {
        static const Pal::BarrierTransition Transition =
        {
            AccelerationStructureCoher,   // srcCacheMask
            Pal::CoherMemory,             // dstCacheMask
            { }                           // imageInfo
        };

        static const Pal::BarrierInfo Barrier =
        {
            Pal::HwPipeTop,                         // waitPoint
            1,                                      // pipePointWaitCount
            &pipePoint,                             // pPipePoints
            0,                                      // gpuEventCount
            nullptr,                                // ppGpuEvents
            0,                                      // rangeCheckedTargetWaitCount
            nullptr,                                // ppTargets
            1,                                      // transitionCount
            &Transition,                            // pTransitions
            0,                                      // globalSrcCacheMask
            0,                                      // globalDstCacheMask
            RgpBarrierInternalPreResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier, m_curDeviceMask);
    }

    utils::IterateMask deviceGroup1(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup1.Index();

        PalCmdBuffer(deviceIdx)->CmdFillMemory(
            accelerationStructureQueryPool.PalMemory(deviceIdx),
            accelerationStructureQueryPool.GetSlotOffset(firstQuery),
            accelerationStructureQueryPool.GetSlotSize() * queryCount,
            0);
    }
    while (deviceGroup1.IterateNext());

    // Wait for memory fill to complete
    {
        static const Pal::BarrierTransition Transition =
        {
            Pal::CoherMemory,             // srcCacheMask
            AccelerationStructureCoher,   // dstCacheMask
            { }                           // imageInfo
        };

        static const Pal::BarrierInfo Barrier =
        {
            Pal::HwPipeTop,                          // waitPoint
            1,                                       // pipePointWaitCount
            &pipePoint,                              // pPipePoints
            0,                                       // gpuEventCount
            nullptr,                                 // ppGpuEvents
            0,                                       // rangeCheckedTargetWaitCount
            nullptr,                                 // ppTargets
            1,                                       // transitionCount
            &Transition,                             // pTransitions
            0,                                       // globalSrcCacheMask
            0,                                       // globalDstCacheMask
            RgpBarrierInternalPostResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier, m_curDeviceMask);
    }
}
#endif

// =====================================================================================================================
void CmdBuffer::FillTimestampQueryPool(
    const TimestampQueryPool& timestampQueryPool,
    const uint32_t firstQuery,
    const uint32_t queryCount,
    const uint32_t timestampChunk)
{
    // All the cache operations operating on the query pool's timestamp memory
    // that may have occurred before/after this reset.
    static const uint32_t TimestampCoher =
        Pal::CoherShaderRead  | // vkCmdCopyQueryPoolResults (CmdDispatch)
        Pal::CoherCopyDst     | // vkCmdResetQueryPool (CmdFillMemory)
        Pal::CoherTimestamp;    // vkCmdWriteTimestamp (CmdWriteTimestamp)

    static const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;

    // Wait for any timestamp query pool events to complete prior to filling memory
    {
        static const Pal::BarrierTransition Transition =
        {
            TimestampCoher,   // srcCacheMask
            Pal::CoherMemory, // dstCacheMask
            { }               // imageInfo
        };

        static const Pal::BarrierInfo Barrier =
        {
            Pal::HwPipeTop,                         // waitPoint
            1,                                      // pipePointWaitCount
            &pipePoint,                             // pPipePoints
            0,                                      // gpuEventCount
            nullptr,                                // ppGpuEvents
            0,                                      // rangeCheckedTargetWaitCount
            nullptr,                                // ppTargets
            1,                                      // transitionCount
            &Transition,                            // pTransitions
            0,                                      // globalSrcCacheMask
            0,                                      // globalDstCacheMask
            RgpBarrierInternalPreResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier, m_curDeviceMask);
    }

    // +----------------+----------------+
    // | TimestampChunk | TimestampChunk |
    // |----------------+----------------|
    // |         TimestampValue          |
    // +---------------------------------+
    // TimestampValue = (uint64_t(TimestampChunk) << 32) + TimestampChunk
    //
    // Write TimestampValue to all timestamps in TimestampQueryPool.
    // Note that each slot in TimestampQueryPool contains only timestamp value.
    // The availability info is generated on the fly from timestamp value.

    utils::IterateMask deviceGroup1(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup1.Index();

        PalCmdBuffer(deviceIdx)->CmdFillMemory(
            timestampQueryPool.PalMemory(deviceIdx),
            timestampQueryPool.GetSlotOffset(firstQuery),
            timestampQueryPool.GetSlotSize() * queryCount,
            timestampChunk);
    }
    while (deviceGroup1.IterateNext());

    // Wait for memory fill to complete
    {
        static const Pal::BarrierTransition Transition =
        {
            Pal::CoherMemory, // srcCacheMask
            TimestampCoher,   // dstCacheMask
            { }               // imageInfo
        };

        static const Pal::BarrierInfo Barrier =
        {
            Pal::HwPipeTop,                          // waitPoint
            1,                                       // pipePointWaitCount
            &pipePoint,                              // pPipePoints
            0,                                       // gpuEventCount
            nullptr,                                 // ppGpuEvents
            0,                                       // rangeCheckedTargetWaitCount
            nullptr,                                 // ppTargets
            1,                                       // transitionCount
            &Transition,                             // pTransitions
            0,                                       // globalSrcCacheMask
            0,                                       // globalDstCacheMask
            RgpBarrierInternalPostResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier, m_curDeviceMask);
    }
}

// =====================================================================================================================
void CmdBuffer::ResetQueryPool(
    VkQueryPool queryPool,
    uint32_t    firstQuery,
    uint32_t    queryCount)
{
    DbgBarrierPreCmd(DbgBarrierQueryReset);

    PalCmdSuspendPredication(true);

    const QueryPool* pBasePool = QueryPool::ObjectFromHandle(queryPool);

    if (pBasePool->GetQueryType() == VK_QUERY_TYPE_TIMESTAMP)
    {
        const TimestampQueryPool* pQueryPool = pBasePool->AsTimestampQueryPool();

        // Write TimestampNotReady to all timestamps in TimestampQueryPool.
        FillTimestampQueryPool(
            *pQueryPool,
            firstQuery,
            queryCount,
            TimestampQueryPool::TimestampNotReadyChunk);
    }
#if VKI_RAY_TRACING
    else if (IsAccelerationStructureQueryType(pBasePool->GetQueryType()))
    {
        const AccelerationStructureQueryPool* pQueryPool = pBasePool->AsAccelerationStructureQueryPool();

        ResetAccelerationStructureQueryPool(
            *pQueryPool,
            firstQuery,
            queryCount);
    }
#endif
    else
    {
        const PalQueryPool* pQueryPool = pBasePool->AsPalQueryPool();

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdResetQueryPool(
                *pQueryPool->PalPool(deviceIdx),
                firstQuery,
                queryCount);
        }
        while (deviceGroup.IterateNext());
    }

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierQueryReset);
}

// =====================================================================================================================
// This is the main hook for any CmdBarrier going into PAL.  Always call this function instead of CmdBarrier directly.
void CmdBuffer::PalCmdBarrier(
    const Pal::BarrierInfo& info,
    uint32_t                deviceMask)
{
    // If you trip this assert, you've forgotten to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block your main code
    // change.
    VK_ASSERT(info.reason != 0);

#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32_t i = 0; i < info.transitionCount; ++i)
    {
        // Detect if PAL may execute a barrier blt using this image
        VK_ASSERT(info.pTransitions[i].imageInfo.pImage == nullptr);
        // You need to use the other PalCmdBarrier method (below) which uses vk::Image ptrs to obtain the
        // corresponding Pal::IImage ptr for each image transition
    }
#endif

    if (m_flags.useReleaseAcquire)
    {
        // Translate the Pal::BarrierInfo to an equivalent Pal::AcquireReleaseInfo struct and then call
        // Pal::CmdReleaseThenAcquire() instead of Pal::CmdBarrier()
        TranslateBarrierInfoToAcqRel(info, deviceMask);
    }
    else
    {
        utils::IterateMask deviceGroup(deviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdBarrier(info);
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBarrier(
    Pal::BarrierInfo*             pInfo,
    Pal::BarrierTransition* const pTransitions,
    const Image** const           pTransitionImages,
    uint32_t                      deviceMask)
{
    // If you trip this assert, you've forgot to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block you.
    VK_ASSERT(pInfo->reason != 0);

    const Pal::IGpuEvent** ppOriginalGpuEvents = pInfo->ppGpuEvents;

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // TODO: I have proposed a better solution with the Pal team. ie grouped images referenced from
        // Pal::BarrierTransition. Executebarriers already wrote the correct Pal::IImage for device 0, so this loop
        // needs to update the Pal::Image* after the first iteration.

        if (deviceIdx > 0)
        {
            for (uint32_t i = 0; i < pInfo->transitionCount; i++)
            {
                if (pTransitions[i].imageInfo.pImage != nullptr)
                {
                    pTransitions[i].imageInfo.pImage = pTransitionImages[i]->PalImage(deviceIdx);
                }
            }
            pInfo->pTransitions = pTransitions;

            // Access the correct Gpu Events for this Pal device
            if (pInfo->ppGpuEvents != nullptr)
            {
                pInfo->ppGpuEvents = &ppOriginalGpuEvents[(pInfo->gpuEventWaitCount * deviceIdx)];
            }
        }

        PalCmdBuffer(deviceIdx)->CmdBarrier(*pInfo);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Translates the Pal::BarrierInfo into equivalent Pal::AcquireReleaseInfo struct. This function does a 1-to-1 mapping
// for struct members and hence should not be used in general.
void CmdBuffer::TranslateBarrierInfoToAcqRel(
    const Pal::BarrierInfo& barrierInfo,
    uint32_t                deviceMask)
{
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    Pal::AcquireReleaseInfo info = {};

    Pal::MemBarrier memoryBarriers = {};

    uint32_t srcStageMask       = 0;
    const uint32_t dstStageMask = ConvertWaitPointToPipeStage(barrierInfo.waitPoint);

    for (uint32_t i = 0; i < barrierInfo.pipePointWaitCount; i++)
    {
        srcStageMask |= ConvertPipePointToPipeStage(barrierInfo.pPipePoints[i]);
    }

    info.reason = barrierInfo.reason;

    // If the transition count is 0 then this barrier is used only for global
    if (barrierInfo.transitionCount == 0)
    {
        info.srcGlobalStageMask  = srcStageMask;
        info.dstGlobalStageMask  = dstStageMask;
        info.srcGlobalAccessMask = barrierInfo.globalSrcCacheMask;
        info.dstGlobalAccessMask = barrierInfo.globalDstCacheMask;
    }
    else
    {
        VK_ASSERT((barrierInfo.globalSrcCacheMask == 0) && (barrierInfo.globalDstCacheMask == 0));

        for (uint32_t i = 0; i < barrierInfo.transitionCount; i++)
        {
            VK_ASSERT(barrierInfo.pTransitions[i].imageInfo.pImage == nullptr);

            // Pal::AcquireReleaseInfo::MemBarrier is valid only for buffers. For renderpasses we would need to
            // use image barriers but since we don't have any information about the relevant Pal::IImage object, the
            // best we can do is record the transition via global memory barrier.
            if (info.reason == RgpBarrierExternalRenderPassSync)
            {
                info.srcGlobalStageMask = srcStageMask;
                info.dstGlobalStageMask = dstStageMask;
                info.srcGlobalAccessMask |= barrierInfo.pTransitions[i].srcCacheMask;
                info.dstGlobalAccessMask |= barrierInfo.pTransitions[i].dstCacheMask;
            }
            else
            {
                memoryBarriers.srcStageMask = srcStageMask;
                memoryBarriers.dstStageMask = dstStageMask;
                memoryBarriers.srcAccessMask |= barrierInfo.pTransitions[i].srcCacheMask;
                memoryBarriers.dstAccessMask |= barrierInfo.pTransitions[i].dstCacheMask;

                // Just passing 1 memory barrier count and OR'ing the cache masks is enough for PAL.
                info.memoryBarrierCount = 1;
            }
        }
    }

    if (info.memoryBarrierCount > 0)
    {
        info.pMemoryBarriers = &memoryBarriers;
    }

    PalCmdReleaseThenAcquire(info, deviceMask);
}

// =====================================================================================================================
// This is the main hook for any CmdReleaseThenAcquire going into PAL. Always call this function instead of
// CmdReleaseThenAcquire directly.
void CmdBuffer::PalCmdReleaseThenAcquire(
    const Pal::AcquireReleaseInfo& info,
    uint32_t                       deviceMask)
{
    // If you trip this assert, you've forgotten to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block your main code change.
    VK_ASSERT(info.reason != 0);

#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32_t i = 0; i < info.imageBarrierCount; ++i)
    {
        // Detect if PAL may execute a barrier blt using this image
        VK_ASSERT(info.pImageBarriers[i].pImage == nullptr);
        // You need to use the other PalCmdReleaseThenAcquire method (below) which uses vk::Image ptrs to obtain the
        // corresponding Pal::IImage ptr for each image transition
    }
#endif

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdReleaseThenAcquire(info);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdReleaseThenAcquire(
    Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
    Pal::MemBarrier* const   pBufferBarriers,
    const Buffer**   const   ppBuffers,
    Pal::ImgBarrier* const   pImageBarriers,
    const Image**    const   ppImages,
    uint32_t                 deviceMask)
{
    // If you trip this assert, you've forgotten to populate a value for this field. You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h. Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block you.
    VK_ASSERT(pAcquireReleaseInfo->reason != 0);

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < pAcquireReleaseInfo->imageBarrierCount; i++)
        {
            if (ppImages != nullptr)
            {
                pImageBarriers[i].pImage = ppImages[i]->PalImage(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pImageBarriers = pImageBarriers;

        for (uint32_t i = 0; i < pAcquireReleaseInfo->memoryBarrierCount; i++)
        {
            if (ppBuffers != nullptr)
            {
                pBufferBarriers[i].memory.address = ppBuffers[i]->GpuVirtAddr(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pMemoryBarriers = pBufferBarriers;

        PalCmdBuffer(deviceIdx)->CmdReleaseThenAcquire(*pAcquireReleaseInfo);
    }
    while (deviceGroup.IterateNext());
}
// =====================================================================================================================
void CmdBuffer::PalCmdAcquire(
    Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
    uint32_t                 eventCount,
    const VkEvent*           pEvents,
    Pal::MemBarrier* const   pBufferBarriers,
    const Buffer**   const   ppBuffers,
    Pal::ImgBarrier* const   pImageBarriers,
    const Image**    const   ppImages,
    VirtualStackFrame*       pVirtStackFrame,
    uint32_t                 deviceMask)
{
    // If you trip this assert, you've forgot to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block you.
    VK_ASSERT(pAcquireReleaseInfo->reason != 0);

    Event* pEvent = Event::ObjectFromHandle(pEvents[0]);

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < pAcquireReleaseInfo->imageBarrierCount; i++)
        {
            if (ppImages != nullptr)
            {
                pImageBarriers[i].pImage = ppImages[i]->PalImage(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pImageBarriers = pImageBarriers;

        for (uint32_t i = 0; i < pAcquireReleaseInfo->memoryBarrierCount; i++)
        {
            if (ppBuffers != nullptr)
            {
                pBufferBarriers[i].memory.address = ppBuffers[i]->GpuVirtAddr(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pMemoryBarriers = pBufferBarriers;

        if (pEvent->IsUseToken())
        {
            // Allocate space to store sync token values (automatically rewound on unscope)
            uint32* pSyncTokens = eventCount > 0 ? pVirtStackFrame->AllocArray<uint32>(eventCount) : nullptr;

            if (pSyncTokens != nullptr)
            {
                for (uint32_t i = 0; i < eventCount; ++i)
                {
                    pSyncTokens[i] = Event::ObjectFromHandle(pEvents[i])->GetSyncToken();
                }

                PalCmdBuffer(deviceIdx)->CmdAcquire(*pAcquireReleaseInfo, eventCount, pSyncTokens);

                pVirtStackFrame->FreeArray(pSyncTokens);
            }
            else
            {
                m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
        else
        {
            // Allocate space to store signaled event pointers (automatically rewound on unscope)
            const Pal::IGpuEvent** ppGpuEvents = eventCount > 0 ?
                pVirtStackFrame->AllocArray<const Pal::IGpuEvent*>(eventCount) : nullptr;

            if (ppGpuEvents != nullptr)
            {
                for (uint32_t i = 0; i < eventCount; ++i)
                {
                    ppGpuEvents[i] = Event::ObjectFromHandle(pEvents[i])->PalEvent(deviceIdx);
                }

                PalCmdBuffer(deviceIdx)->CmdAcquireEvent(*pAcquireReleaseInfo, eventCount, ppGpuEvents);

                pVirtStackFrame->FreeArray(ppGpuEvents);
            }
            else
            {
                m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdRelease(
    Pal::AcquireReleaseInfo* pAcquireReleaseInfo,
    uint32_t                 eventCount,
    const VkEvent*           pEvents,
    Pal::MemBarrier* const   pBufferBarriers,
    const Buffer**   const   ppBuffers,
    Pal::ImgBarrier* const   pImageBarriers,
    const Image**    const   ppImages,
    uint32_t                 deviceMask)
{
    // If you trip this assert, you've forgot to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block you.
    VK_ASSERT(pAcquireReleaseInfo->reason != 0);

    VK_ASSERT(eventCount == 1);

    Event* pEvent = Event::ObjectFromHandle(*pEvents);

    utils::IterateMask deviceGroup(deviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < pAcquireReleaseInfo->imageBarrierCount; i++)
        {
            if (ppImages != nullptr)
            {
                pImageBarriers[i].pImage = ppImages[i]->PalImage(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pImageBarriers = pImageBarriers;

        for (uint32_t i = 0; i < pAcquireReleaseInfo->memoryBarrierCount; i++)
        {
            if (ppBuffers != nullptr)
            {
                pBufferBarriers[i].memory.address = ppBuffers[i]->GpuVirtAddr(deviceIdx);
            }
        }
        pAcquireReleaseInfo->pMemoryBarriers = pBufferBarriers;

        if (pEvent->IsUseToken())
        {
            pEvent->SetSyncToken(PalCmdBuffer(deviceIdx)->CmdRelease(*pAcquireReleaseInfo));
        }
        else
        {
            PalCmdBuffer(deviceIdx)->CmdReleaseEvent(*pAcquireReleaseInfo, pEvent->PalEvent(deviceIdx));
        }
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindMsaaStates(
    const Pal::IMsaaState* const * pStates)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBindMsaaState(PalCmdBuffer(deviceIdx), deviceIdx, (pStates != nullptr) ? pStates[deviceIdx] : nullptr);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdSetMsaaQuadSamplePattern(
    uint32_t                          numSamplesPerPixel,
    const Pal::MsaaQuadSamplePattern& quadSamplePattern)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        PalCmdBuffer(deviceIdx)->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::PalCmdSuspendPredication(
    bool suspend)
{
    if (m_flags.hasConditionalRendering)
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            PalCmdBuffer(deviceIdx)->CmdSuspendPredication(suspend);
        }
        while (deviceGroup.IterateNext());
    }
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

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdResolveQuery(
                *pPool->PalPool(deviceIdx),
                VkToPalQueryResultFlags(flags),
                pPool->PalQueryType(),
                firstQuery,
                queryCount,
                *pDestBuffer->PalMemory(deviceIdx),
                pDestBuffer->MemOffset() + destOffset,
                destStride);
        }
        while (deviceGroup.IterateNext());
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
    const QueryPool*   pBasePool,
    const Buffer*      pDestBuffer,
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
    const uint32_t storageViewSize   = m_pDevice->GetProperties().descriptorSizes.bufferView;
    const uint32_t storageViewDwSize = storageViewSize / sizeof(uint32_t);
    const uint32_t viewOffset        = 0;
    const uint32_t bufferViewOffset  = storageViewDwSize;
    const uint32_t queryCountOffset  = bufferViewOffset + storageViewDwSize;
    const uint32_t copyFlagsOffset   = queryCountOffset + 1;
    const uint32_t copyStrideOffset  = copyFlagsOffset + 1;
    const uint32_t firstQueryOffset  = copyStrideOffset + 1;
    const uint32_t ptrQueryOffset    = firstQueryOffset + 1;
    const uint32_t userDataCount     = ptrQueryOffset + 1;

    // Make sure they agree with pipeline mapping
    VK_ASSERT(viewOffset == pipeline.userDataNodeOffsets[0]);
    VK_ASSERT(bufferViewOffset == pipeline.userDataNodeOffsets[1]);
    VK_ASSERT(queryCountOffset == pipeline.userDataNodeOffsets[2]);
    VK_ASSERT(userDataCount <= VK_ARRAY_SIZE(userData));

    // Create and set a raw storage view into the destination buffer (shader will choose to either write 32-bit or
    // 64-bit values)
    Pal::BufferViewInfo bufferViewInfo = {};

    bufferViewInfo.range = destStride * queryCount;
    bufferViewInfo.stride = 0; // Raw buffers have a zero byte stride
    bufferViewInfo.swizzledFormat = Pal::UndefinedSwizzledFormat;

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
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Command to write a timestamp value to a location in a Timestamp query pool
void CmdBuffer::WriteTimestamp(
    PipelineStageFlags        pipelineStage,
    const TimestampQueryPool* pQueryPool,
    uint32_t                  query)
{
    DbgBarrierPreCmd(DbgBarrierWriteTimestamp);

    PalCmdSuspendPredication(true);

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdWriteTimestamp(
            VkToPalSrcPipePointForTimestampWrite(pipelineStage),
            pQueryPool->PalMemory(deviceIdx),
            pQueryPool->GetSlotOffset(query));

        const auto* const pRenderPass = m_allGpuState.pRenderPass;

        // If vkCmdWriteTimestamp is called while executing a render pass instance that has multiview enabled,
        // the timestamp uses N consecutive query indices in the query pool (starting at query) where
        // N is the number of bits set in the view mask of the subpass the command is executed in.
        //
        // The first query is a timestamp value and (if more than one bit is set in the view mask)
        // zero is written to the remaining queries.
        if (((pRenderPass != nullptr) && pRenderPass->IsMultiviewEnabled()) ||
            (m_allGpuState.dynamicRenderingInstance.viewMask != 0))
        {
            const auto viewMask = (pRenderPass != nullptr) ? pRenderPass->GetViewMask(m_renderPassInstance.subpass) :
                m_allGpuState.dynamicRenderingInstance.viewMask;
            const auto viewCount = Util::CountSetBits(viewMask);

            VK_ASSERT(viewCount > 0);
            const auto remainingQueryCount = viewCount - 1;

            if (remainingQueryCount > 0)
            {
                const auto firstRemainingQuery = query + 1;
                constexpr uint32_t TimestampZeroChunk = 0;

                // Set value of each remaining query to 0 and to make them avaliable.
                // Note that values of remaining queries (to which 0 was written) are not considered timestamps.
                FillTimestampQueryPool(
                   *pQueryPool,
                    firstRemainingQuery,
                    remainingQueryCount,
                    TimestampZeroChunk);
            }
        }
    }
    while (deviceGroup.IterateNext());

    PalCmdSuspendPredication(false);

    DbgBarrierPostCmd(DbgBarrierWriteTimestamp);
}

// =====================================================================================================================
void CmdBuffer::SetSampleLocations(
    const VkSampleLocationsInfoEXT* pSampleLocationsInfo)
{
    uint32_t sampleLocationsPerPixel = static_cast<uint32_t>(pSampleLocationsInfo->sampleLocationsPerPixel);

    if (sampleLocationsPerPixel > 0)
    {
        ConvertToPalMsaaQuadSamplePattern(pSampleLocationsInfo, &m_allGpuState.samplePattern.locations);
    }

    m_allGpuState.samplePattern.sampleCount = sampleLocationsPerPixel;
    m_allGpuState.dirtyGraphics.samplePattern = 1;
}

// =====================================================================================================================
// Begins a render pass instance (vkCmdBeginRenderPass)
void CmdBuffer::BeginRenderPass(
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents            contents)
{
    DbgBarrierPreCmd(DbgBarrierBeginRenderPass);

    m_allGpuState.pRenderPass  = RenderPass::ObjectFromHandle(pRenderPassBegin->renderPass);
    m_allGpuState.pFramebuffer = Framebuffer::ObjectFromHandle(pRenderPassBegin->framebuffer);

    Pal::Result result = Pal::Result::Success;

    EXTRACT_VK_STRUCTURES_3(
        RP,
        RenderPassBeginInfo,
        DeviceGroupRenderPassBeginInfo,
        RenderPassSampleLocationsBeginInfoEXT,
        RenderPassAttachmentBeginInfo,
        pRenderPassBegin,
        RENDER_PASS_BEGIN_INFO,
        DEVICE_GROUP_RENDER_PASS_BEGIN_INFO,
        RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT,
        RENDER_PASS_ATTACHMENT_BEGIN_INFO)

    // Copy render areas (these may be per-device in a group)
    bool replicateRenderArea = true;

    // Set the render pass instance's device mask to the value the command buffer began with.
    SetRpDeviceMask(m_cbBeginDeviceMask);

    if (pDeviceGroupRenderPassBeginInfo != nullptr)
    {
        SetRpDeviceMask(pDeviceGroupRenderPassBeginInfo->deviceMask);

        SetDeviceMask(GetRpDeviceMask());

        m_renderPassInstance.renderAreaCount = pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount;

        VK_ASSERT(m_renderPassInstance.renderAreaCount <= MaxPalDevices);

        if (pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount > 0)
        {
            utils::IterateMask deviceGroup(pDeviceGroupRenderPassBeginInfo->deviceMask);

            VK_ASSERT(m_numPalDevices == pDeviceGroupRenderPassBeginInfo->deviceRenderAreaCount);

            do
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                const VkRect2D& srcRect  = pDeviceGroupRenderPassBeginInfo->pDeviceRenderAreas[deviceIdx];
                auto*           pDstRect = &m_renderPassInstance.renderArea[deviceIdx];

                pDstRect->offset.x      = srcRect.offset.x;
                pDstRect->offset.y      = srcRect.offset.y;
                pDstRect->extent.width  = srcRect.extent.width;
                pDstRect->extent.height = srcRect.extent.height;
            }
            while (deviceGroup.IterateNext());

            replicateRenderArea = false;
        }
    }

    if (replicateRenderArea)
    {
        m_renderPassInstance.renderAreaCount = m_numPalDevices;

        const auto& srcRect = pRenderPassBeginInfo->renderArea;

        for (uint32_t deviceIdx = 0; deviceIdx <  m_numPalDevices; deviceIdx++)
        {
            auto* pDstRect          = &m_renderPassInstance.renderArea[deviceIdx];

            pDstRect->offset.x      = srcRect.offset.x;
            pDstRect->offset.y      = srcRect.offset.y;
            pDstRect->extent.width  = srcRect.extent.width;
            pDstRect->extent.height = srcRect.extent.height;
        }
    }

    const uint32_t attachmentCount = m_allGpuState.pRenderPass->GetAttachmentCount();

    // Allocate enough per-attachment state space
    if (m_renderPassInstance.maxAttachmentCount < attachmentCount)
    {
        // Free old memory
        if (m_renderPassInstance.pAttachments != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pAttachments);

            m_renderPassInstance.pAttachments       = nullptr;
            m_renderPassInstance.maxAttachmentCount = 0;
        }

        // Allocate enough to cover new requirements
        const size_t maxAttachmentCount = Util::Max(attachmentCount, 8U);

        m_renderPassInstance.pAttachments = static_cast<RenderPassInstanceState::AttachmentState*>(
            m_pDevice->VkInstance()->AllocMem(
                sizeof(RenderPassInstanceState::AttachmentState) * maxAttachmentCount,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (m_renderPassInstance.pAttachments != nullptr)
        {
            m_renderPassInstance.maxAttachmentCount = maxAttachmentCount;
            memset(m_renderPassInstance.pAttachments, 0,
                maxAttachmentCount * sizeof(RenderPassInstanceState::AttachmentState));
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    const uint32_t subpassCount = m_allGpuState.pRenderPass->GetSubpassCount();

    // Allocate pSamplePatterns memory
    if (m_renderPassInstance.maxSubpassCount < subpassCount)
    {
        // Free old memory
        if (m_renderPassInstance.pSamplePatterns != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pSamplePatterns);

            m_renderPassInstance.pSamplePatterns = nullptr;
            m_renderPassInstance.maxSubpassCount = 0;
        }

        // Allocate enough to cover new requirements
        m_renderPassInstance.pSamplePatterns = static_cast<SamplePattern*>(
            m_pDevice->VkInstance()->AllocMem(
                sizeof(SamplePattern) * subpassCount,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (m_renderPassInstance.pSamplePatterns != nullptr)
        {
            m_renderPassInstance.maxSubpassCount = subpassCount;
            memset(m_renderPassInstance.pSamplePatterns, 0, subpassCount * sizeof(SamplePattern));
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    if (pRenderPassAttachmentBeginInfo != nullptr)
    {
        if (m_allGpuState.pFramebuffer->Imageless() == false)
        {
            VK_ASSERT(pRenderPassAttachmentBeginInfo->attachmentCount == 0);
        }
        else
        {
            VK_ASSERT(pRenderPassAttachmentBeginInfo->attachmentCount == attachmentCount);
            VK_ASSERT(pRenderPassAttachmentBeginInfo->attachmentCount ==
                    m_allGpuState.pFramebuffer->GetAttachmentCount());
        }

        m_allGpuState.pFramebuffer->SetImageViews(pRenderPassAttachmentBeginInfo);
    }

    if (result == Pal::Result::Success)
    {
        m_renderPassInstance.subpass = 0;

        // Copy clear values
        if (pRenderPassBeginInfo->pClearValues != nullptr)
        {
            const uint32_t clearValueCount = Util::Min(pRenderPassBeginInfo->clearValueCount, attachmentCount);

            for (uint32_t a = 0; a < clearValueCount; ++a)
            {
                m_renderPassInstance.pAttachments[a].clearValue = pRenderPassBeginInfo->pClearValues[a];
            }
        }

        // Initialize current layout state based on attachment initial layout
        for (uint32_t a = 0; a < attachmentCount; ++a)
        {
            // Start current layouts to PAL version of initial layout for each attachment.
            constexpr Pal::ImageLayout NullLayout     = {};
            const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(a);
            const uint32 firstPlane = attachment.subresRange[0].startSubres.plane;

            const RPImageLayout initialLayout =
            { m_allGpuState.pRenderPass->GetAttachmentDesc(a).initialLayout, 0 };

            if (!attachment.pImage->IsDepthStencilFormat())
            {
                RPSetAttachmentLayout(
                    a,
                    firstPlane,
                    attachment.pImage->GetAttachmentLayout(initialLayout, firstPlane, this));
            }
            else
            {
                // Note that we set both depth and stencil aspect layouts for depth/stencil formats to define
                // initial values for them.  This avoids some (incorrect) PAL asserts when clearing depth- or
                // stencil-only surfaces.  Here, the missing aspect will have a null usage but a non-null engine
                // component.
                VK_ASSERT((firstPlane == 0) || (firstPlane == 1));

                const RPImageLayout initialStencilLayout =
                { m_allGpuState.pRenderPass->GetAttachmentDesc(a).stencilInitialLayout, 0 };

                RPSetAttachmentLayout(
                    a,
                    0,
                    attachment.pImage->GetAttachmentLayout(initialLayout, 0, this));

                RPSetAttachmentLayout(
                    a,
                    1,
                    attachment.pImage->GetAttachmentLayout(initialStencilLayout, 1, this));
            }
        }

        if (pRenderPassSampleLocationsBeginInfoEXT != nullptr)
        {
            uint32_t attachmentInitialSampleLocationCount =
                pRenderPassSampleLocationsBeginInfoEXT->attachmentInitialSampleLocationsCount;

            for (uint32_t ai = 0; ai < attachmentInitialSampleLocationCount; ++ai)
            {
                const uint32_t attachmentIndex =
                    pRenderPassSampleLocationsBeginInfoEXT->pAttachmentInitialSampleLocations[ai].attachmentIndex;

                VK_ASSERT(attachmentIndex < attachmentCount);
                const Framebuffer::Attachment& attachment =
                    m_allGpuState.pFramebuffer->GetAttachment(attachmentIndex);

                if (attachment.pImage->IsSampleLocationsCompatibleDepth())
                {
                    const VkSampleLocationsInfoEXT* pSampleLocationsInfo =
                        &pRenderPassSampleLocationsBeginInfoEXT->pAttachmentInitialSampleLocations[ai].sampleLocationsInfo;

                    m_renderPassInstance.pAttachments[attachmentIndex].initialSamplePattern.sampleCount =
                        (uint32_t)pSampleLocationsInfo->sampleLocationsPerPixel;

                    ConvertToPalMsaaQuadSamplePattern(
                        pSampleLocationsInfo,
                        &m_renderPassInstance.pAttachments[attachmentIndex].initialSamplePattern.locations);
                }
            }

            uint32_t postSubpassSampleLocationsCount =
                pRenderPassSampleLocationsBeginInfoEXT->postSubpassSampleLocationsCount;

            for (uint32_t ps = 0; ps < postSubpassSampleLocationsCount; ++ps)
            {
                const uint32_t psIndex =
                    pRenderPassSampleLocationsBeginInfoEXT->pPostSubpassSampleLocations[ps].subpassIndex;

                const VkSampleLocationsInfoEXT* pSampleLocationsInfo =
                    &pRenderPassSampleLocationsBeginInfoEXT->pPostSubpassSampleLocations[ps].sampleLocationsInfo;

                m_renderPassInstance.pSamplePatterns[psIndex].sampleCount =
                    (uint32_t)pSampleLocationsInfo->sampleLocationsPerPixel;

                ConvertToPalMsaaQuadSamplePattern(
                    pSampleLocationsInfo,
                    &m_renderPassInstance.pSamplePatterns[psIndex].locations);
            }
        }

        // Begin the first subpass
        m_renderPassInstance.pExecuteInfo = m_allGpuState.pRenderPass->GetExecuteInfo();

        utils::IterateMask deviceGroup(GetRpDeviceMask());
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            PalCmdBuffer(deviceIdx)->CmdSetGlobalScissor(m_allGpuState.pFramebuffer->GetGlobalScissorParams());
        }
        while (deviceGroup.IterateNext());

        RPBeginSubpass();

    }
    else
    {
        // Set a dummy state such that other instance commands ignore the render pass instance.
        m_renderPassInstance.subpass = VK_SUBPASS_EXTERNAL;
    }

    DbgBarrierPostCmd(DbgBarrierBeginRenderPass);
}

// =====================================================================================================================
// Advances to the next sub-pass in the current render pass (vkCmdNextSubPass)
void CmdBuffer::NextSubPass(
    VkSubpassContents      contents)
{
    DbgBarrierPreCmd(DbgBarrierNextSubpass);

    if (m_renderPassInstance.subpass != VK_SUBPASS_EXTERNAL)
    {

        // End the previous subpass
        RPEndSubpass();

        // Advance the current subpass index
        m_renderPassInstance.subpass++;

        // Begin the next subpass
        RPBeginSubpass();

    }

    DbgBarrierPostCmd(DbgBarrierNextSubpass);
}

// =====================================================================================================================
// Ends the current subpass during a render pass instance.
void CmdBuffer::RPEndSubpass()
{
    VK_ASSERT(m_renderPassInstance.subpass < m_allGpuState.pRenderPass->GetSubpassCount());

    // Get current subpass execute state
    const auto& subpass = m_renderPassInstance.pExecuteInfo->pSubpasses[m_renderPassInstance.subpass];

    VirtualStackFrame virtStack(m_pStackAllocator);

    // Synchronize preceding work before resolving if needed
    if (subpass.end.syncPreResolve.flags.active)
    {
        RPSyncPoint(subpass.end.syncPreResolve, &virtStack);
    }

    // Execute any multisample resolve attachment operations
    if (subpass.end.resolveCount > 0)
    {
        RPResolveAttachments(subpass.end.resolveCount, subpass.end.pResolves);
    }

    // Synchronize preceding work at the end of the subpass
    if (subpass.end.syncBottom.flags.active)
    {
        RPSyncPoint(subpass.end.syncBottom, &virtStack);
    }
}

// =====================================================================================================================
// Handles post-clear synchronization for load-op color clears when not auto-syncing.
void CmdBuffer::RPSyncPostLoadOpColorClear()
{
    static const Pal::BarrierTransition transition =
    {
        Pal::CoherClear,
        Pal::CoherColorTarget,
        {}
    };

    static const Pal::HwPipePoint PipePoint = Pal::HwPipePostBlt;
    static const Pal::BarrierInfo Barrier   =
    {
        Pal::HwPipePreRasterization,        // waitPoint
        1,                                  // pipePointWaitCount
        &PipePoint,                         // pPipePoints
        0,                                  // gpuEventWaitCount
        nullptr,                            // ppGpuEvents
        0,                                  // rangeCheckedTargetWaitCount
        nullptr,                            // ppTargets
        1,                                  // transitionCount
        &transition,                        // pTransitions
        0,                                  // globalSrcCacheMask
        0,                                  // globalDstCacheMask
        RgpBarrierExternalRenderPassSync    // reason
    };

    PalCmdBarrier(Barrier, GetRpDeviceMask());
}

// =====================================================================================================================
// Begins the current subpass during a render pass instance.
void CmdBuffer::RPBeginSubpass()
{
    VK_ASSERT(m_renderPassInstance.subpass < m_allGpuState.pRenderPass->GetSubpassCount());

    // Get current subpass execute state
    const auto& subpass = m_renderPassInstance.pExecuteInfo->pSubpasses[m_renderPassInstance.subpass];

    // Synchronize prior work (defined by subpass dependencies) prior to the top of this subpass, and handle any
    // layout transitions for this subpass's references.
    if (subpass.begin.syncTop.flags.active)
    {
        VirtualStackFrame virtStack(m_pStackAllocator);
        RPSyncPoint(subpass.begin.syncTop, &virtStack);
    }

    if (m_flags.subpassLoadOpClearsBoundAttachments)
    {
        // Bind targets
        RPBindTargets(subpass.begin.bindTargets);
    }

    if ((subpass.begin.loadOps.colorClearCount > 0) ||
        (subpass.begin.loadOps.dsClearCount > 0))
    {
        PalCmdSuspendPredication(true);

        // Execute any color clear load operations
        if (subpass.begin.loadOps.colorClearCount > 0)
        {
            RPLoadOpClearColor(subpass.begin.loadOps.colorClearCount, subpass.begin.loadOps.pColorClears);
        }

        // If we are manually pre-syncing color clears, we must post-sync also
        if (subpass.begin.syncTop.barrier.flags.preColorClearSync)
        {
            RPSyncPostLoadOpColorClear();
        }

        // Execute any depth-stencil clear load operations
        if (subpass.begin.loadOps.dsClearCount > 0)
        {
            RPLoadOpClearDepthStencil(subpass.begin.loadOps.dsClearCount, subpass.begin.loadOps.pDsClears);
        }

        PalCmdSuspendPredication(false);
    }

    if (m_flags.subpassLoadOpClearsBoundAttachments == false)
    {
        // Bind targets
        RPBindTargets(subpass.begin.bindTargets);
    }

    // Set view instance mask, on devices in render pass instance's device mask
    SetViewInstanceMask(GetRpDeviceMask());

}

// =====================================================================================================================
// Executes a "sync point" during a render pass instance using the legacy barriers. There are a number of these at
// different stages between subpasses where we handle execution/memory dependencies from subpass dependencies as well as
// trigger automatic layout transitions.
void CmdBuffer::RPSyncPointLegacy(
    const RPSyncPointInfo& syncPoint,
    VirtualStackFrame*     pVirtStack)
{
    const auto& rpBarrier = syncPoint.barrier;

    Pal::BarrierInfo barrier = {};

    barrier.reason             = RgpBarrierExternalRenderPassSync;
    barrier.waitPoint          = rpBarrier.waitPoint;
    barrier.pipePointWaitCount = rpBarrier.pipePointCount;
    barrier.pPipePoints        = rpBarrier.pipePoints;

    const uint32_t maxTransitionCount = MaxPalAspectsPerMask * syncPoint.transitionCount;

    Pal::BarrierTransition* pPalTransitions = (maxTransitionCount != 0)                                          ?
                                              pVirtStack->AllocArray<Pal::BarrierTransition>(maxTransitionCount) :
                                              nullptr;
    const Image** ppImages                  = (maxTransitionCount != 0)                                ?
                                              pVirtStack->AllocArray<const Image*>(maxTransitionCount) :
                                              nullptr;

    // Construct global memory dependency to synchronize caches (subpass dependencies + implicit synchronization)
    if (rpBarrier.flags.needsGlobalTransition)
    {
        Pal::BarrierTransition globalTransition = { };

        m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
            rpBarrier.srcAccessMask,
            rpBarrier.dstAccessMask,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            &globalTransition);

        barrier.globalSrcCacheMask = globalTransition.srcCacheMask | rpBarrier.implicitSrcCacheMask;
        barrier.globalDstCacheMask = globalTransition.dstCacheMask | rpBarrier.implicitDstCacheMask;
    }

    if ((pPalTransitions != nullptr) && (ppImages != nullptr))
    {
        // Construct attachment-specific layout transitions
        for (uint32_t t = 0; t < syncPoint.transitionCount; ++t)
        {
            const RPTransitionInfo& tr = syncPoint.pTransitions[t];

            const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(tr.attachment);

            for (uint32_t sr = 0; sr < attachment.subresRangeCount; ++sr)
            {
                const uint32_t plane = attachment.subresRange[sr].startSubres.plane;

                const RPImageLayout nextLayout = (plane == 1) ? tr.nextStencilLayout : tr.nextLayout;

                const Pal::ImageLayout newLayout = attachment.pImage->GetAttachmentLayout(
                    nextLayout,
                    plane,
                    this);

                const Pal::ImageLayout oldLayout = RPGetAttachmentLayout(
                    tr.attachment,
                    plane);

                if ((oldLayout.usages  != newLayout.usages) ||
                    (oldLayout.engines != newLayout.engines))
                {
                    VK_ASSERT(barrier.transitionCount < maxTransitionCount);

                    ppImages[barrier.transitionCount] = attachment.pImage;

                    Pal::BarrierTransition* pLayoutTransition = &pPalTransitions[barrier.transitionCount++];

                    pLayoutTransition->srcCacheMask                 = 0;
                    pLayoutTransition->dstCacheMask                 = 0;
                    pLayoutTransition->imageInfo.pImage             = attachment.pImage->PalImage(DefaultDeviceIndex);
                    pLayoutTransition->imageInfo.oldLayout          = oldLayout;
                    pLayoutTransition->imageInfo.newLayout          = newLayout;
                    pLayoutTransition->imageInfo.subresRange        = attachment.subresRange[sr];

                    const Pal::MsaaQuadSamplePattern* pQuadSamplePattern = nullptr;

                    if (attachment.pImage->IsSampleLocationsCompatibleDepth() &&
                        tr.flags.isInitialLayoutTransition)
                    {
                        VK_ASSERT(attachment.pImage->HasDepth());

                        // Use the provided sample locations for this attachment if this is its
                        // initial layout transition
                        pQuadSamplePattern =
                            &m_renderPassInstance.pAttachments[tr.attachment].initialSamplePattern.locations;
                    }
                    else
                    {
                        // Otherwise, use the subpass' sample locations
                        uint32_t subpass   = m_renderPassInstance.subpass;
                        pQuadSamplePattern = &m_renderPassInstance.pSamplePatterns[subpass].locations;
                    }

                    pLayoutTransition->imageInfo.pQuadSamplePattern = pQuadSamplePattern;

                    RPSetAttachmentLayout(tr.attachment, plane, newLayout);
                }
            }
        }

        barrier.pTransitions = pPalTransitions;
    }
    else if (maxTransitionCount != 0)
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // If app specifies the src/dst access masks in the subpass dependencies without layout transition at the end of
    // renderpass, cache will not be flushed according to PAL barrier logic, which will cause dirty values in the memory.
    // To fix the above issue, we construct a dumb transition to match PAL's logic to sync cache.
    // Construct a dumb transition to sync cache
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();
    if (settings.enableDumbTransitionSync && (barrier.transitionCount == 0) && (rpBarrier.flags.needsGlobalTransition))
    {
        if (pPalTransitions == nullptr)
        {
            pPalTransitions = pVirtStack->AllocArray<Pal::BarrierTransition>(1);
        }

        if (pPalTransitions != nullptr)
        {
            Pal::BarrierTransition *pDumbTransition = &pPalTransitions[0];
            pDumbTransition->srcCacheMask = 0;
            pDumbTransition->dstCacheMask = 0;
            pDumbTransition->imageInfo.pImage = nullptr;

            barrier.transitionCount = 1;
            barrier.pTransitions = pDumbTransition;
        }
    }

    // Execute the barrier if it actually did anything
    if ((barrier.waitPoint != Pal::HwPipeBottom) ||
        (barrier.transitionCount > 0)            ||
        ((barrier.pipePointWaitCount > 1)        ||
         ((barrier.pipePointWaitCount == 1) && (barrier.pPipePoints[0] != Pal::HwPipeTop))))
    {
        PalCmdBarrier(&barrier, pPalTransitions, ppImages, GetRpDeviceMask());
    }

    if (pPalTransitions != nullptr)
    {
        pVirtStack->FreeArray(pPalTransitions);
    }

    if (ppImages != nullptr)
    {
        pVirtStack->FreeArray(ppImages);
    }
}

// =====================================================================================================================
// Executes a "sync point" during a render pass instance.  There are a number of these at different stages between
// subpasses where we handle execution/memory dependencies from subpass dependencies as well as trigger automatic
// layout transitions.
void CmdBuffer::RPSyncPoint(
    const RPSyncPointInfo& syncPoint,
    VirtualStackFrame*     pVirtStack)
{
    const auto& rpBarrier = syncPoint.barrier;

    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    if (m_flags.useReleaseAcquire)
    {
        Pal::AcquireReleaseInfo acquireReleaseInfo = {};

        acquireReleaseInfo.reason   = RgpBarrierExternalRenderPassSync;

        const uint32_t srcStageMask = VkToPalPipelineStageFlags(rpBarrier.srcStageMask, true);
        const uint32_t dstStageMask = VkToPalPipelineStageFlags(rpBarrier.dstStageMask, false);

        const uint32_t maxTransitionCount = MaxPalAspectsPerMask * syncPoint.transitionCount;

        Pal::ImgBarrier* pPalTransitions  = (maxTransitionCount != 0) ?
                                            pVirtStack->AllocArray<Pal::ImgBarrier>(maxTransitionCount) :
                                            nullptr;
        const Image** ppImages            = (maxTransitionCount != 0)                                ?
                                            pVirtStack->AllocArray<const Image*>(maxTransitionCount) :
                                            nullptr;

        const bool isDstStageNotBottomOfPipe = (dstStageMask != Pal::PipelineStageBottomOfPipe);

        // Construct global memory dependency to synchronize caches (subpass dependencies + implicit synchronization)
        if (rpBarrier.flags.needsGlobalTransition)
        {
            Pal::BarrierTransition globalTransition = { };

            m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
                rpBarrier.srcAccessMask,
                rpBarrier.dstAccessMask,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                &globalTransition);

            acquireReleaseInfo.srcGlobalStageMask  = srcStageMask;
            acquireReleaseInfo.dstGlobalStageMask  = dstStageMask;
            acquireReleaseInfo.srcGlobalAccessMask = globalTransition.srcCacheMask | rpBarrier.implicitSrcCacheMask;
            acquireReleaseInfo.dstGlobalAccessMask = globalTransition.dstCacheMask | rpBarrier.implicitDstCacheMask;
        }

        if ((pPalTransitions != nullptr) && (ppImages != nullptr))
        {
            // Construct attachment-specific layout transitions
            for (uint32_t t = 0; t < syncPoint.transitionCount; ++t)
            {
                const RPTransitionInfo& tr = syncPoint.pTransitions[t];

                const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(tr.attachment);

                Pal::BarrierTransition imageTransition = { };

                m_pDevice->GetBarrierPolicy().ApplyBarrierCacheFlags(
                    rpBarrier.srcAccessMask,
                    rpBarrier.dstAccessMask,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_GENERAL,
                    &imageTransition);

                uint32_t srcAccessMask = imageTransition.srcCacheMask | rpBarrier.implicitSrcCacheMask;
                uint32_t dstAccessMask = imageTransition.dstCacheMask | rpBarrier.implicitDstCacheMask;

                for (uint32_t sr = 0; sr < attachment.subresRangeCount; ++sr)
                {
                    const uint32_t plane = attachment.subresRange[sr].startSubres.plane;

                    const RPImageLayout nextLayout = (plane == 1) ? tr.nextStencilLayout : tr.nextLayout;

                    const Pal::ImageLayout newLayout = attachment.pImage->GetAttachmentLayout(
                        nextLayout,
                        plane,
                        this);

                    const Pal::ImageLayout oldLayout = RPGetAttachmentLayout(
                        tr.attachment,
                        plane);

                    if ((oldLayout.usages  != newLayout.usages)  ||
                        (oldLayout.engines != newLayout.engines) ||
                        ((srcAccessMask    != dstAccessMask) && settings.rpBarrierCheckAccessMasks))
                    {
                        VK_ASSERT(acquireReleaseInfo.imageBarrierCount < maxTransitionCount);

                        ppImages[acquireReleaseInfo.imageBarrierCount] = attachment.pImage;

                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].srcStageMask  = srcStageMask;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].dstStageMask  = dstStageMask;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].srcAccessMask = srcAccessMask;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].dstAccessMask = dstAccessMask;
                        // We set the pImage to nullptr by default here. But, this will be computed correctly later for
                        // each device including DefaultDeviceIndex based on the deviceId.
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].pImage        = nullptr;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].oldLayout     = oldLayout;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].newLayout     = newLayout;
                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].subresRange   = attachment.subresRange[sr];

                        const Pal::MsaaQuadSamplePattern* pQuadSamplePattern = nullptr;

                        if (attachment.pImage->IsSampleLocationsCompatibleDepth() &&
                            tr.flags.isInitialLayoutTransition)
                        {
                            VK_ASSERT(attachment.pImage->HasDepth());

                            // Use the provided sample locations for this attachment if this is its
                            // initial layout transition
                            pQuadSamplePattern =
                                &m_renderPassInstance.pAttachments[tr.attachment].initialSamplePattern.locations;
                        }
                        else
                        {
                            // Otherwise, use the subpass' sample locations
                            uint32_t subpass   = m_renderPassInstance.subpass;
                            pQuadSamplePattern = &m_renderPassInstance.pSamplePatterns[subpass].locations;
                        }

                        pPalTransitions[acquireReleaseInfo.imageBarrierCount].pQuadSamplePattern = pQuadSamplePattern;

                        RPSetAttachmentLayout(tr.attachment, plane, newLayout);

                        acquireReleaseInfo.imageBarrierCount++;
                    }
                }
            }

            acquireReleaseInfo.pImageBarriers = pPalTransitions;
        }
        else if (maxTransitionCount != 0)
        {
            m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        if ((settings.forceDisableGlobalBarrierCacheSync) &&
            (acquireReleaseInfo.imageBarrierCount == 0)   &&
            (acquireReleaseInfo.memoryBarrierCount == 0)  &&
            (rpBarrier.flags.needsGlobalTransition))
        {
            acquireReleaseInfo.srcGlobalAccessMask = 0;
            acquireReleaseInfo.dstGlobalAccessMask = 0;
        }

        // We do not require a dumb transition here in acquire/release interface because unlike Legacy barriers,
        // PAL flushes caches even if only the global barriers are passed-in without any image/buffer memory barriers.

        // Execute the barrier if it actually did anything
        if ((acquireReleaseInfo.dstGlobalStageMask != Pal::PipelineStageBottomOfPipe) ||
            ((acquireReleaseInfo.imageBarrierCount > 0) && isDstStageNotBottomOfPipe) ||
            ((rpBarrier.pipePointCount > 1)                                           ||
             ((rpBarrier.pipePointCount == 1) && (rpBarrier.pipePoints[0] != Pal::HwPipeTop))))
        {
            PalCmdReleaseThenAcquire(
                &acquireReleaseInfo,
                nullptr,
                nullptr,
                pPalTransitions,
                ppImages,
                GetRpDeviceMask());
        }

        if (pPalTransitions != nullptr)
        {
            pVirtStack->FreeArray(pPalTransitions);
        }

        if (ppImages != nullptr)
        {
            pVirtStack->FreeArray(ppImages);
        }
    }
    else
    {
        RPSyncPointLegacy(syncPoint, pVirtStack);
    }
}

// =====================================================================================================================
// Does one or more load-op color clears during a render pass instance.
void CmdBuffer::RPLoadOpClearColor(
    uint32_t                 count,
    const RPLoadOpClearInfo* pClears)
{
    if (m_pSqttState != nullptr)
    {
        m_pSqttState->BeginRenderPassColorClear();
    }

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    Util::Vector<Pal::ClearBoundTargetRegion, 8, VirtualStackFrame> clearRegions{ &virtStackFrame };

    const auto maxRects  = EstimateMaxObjectsOnVirtualStack(sizeof(VkClearRect));
    auto       rectBatch = Util::Min(count, maxRects);
    const auto palResult = clearRegions.Reserve(rectBatch);

    VK_ASSERT(palResult == Pal::Result::Success);

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPLoadOpClearInfo& clear = pClears[i];

        const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(clear.attachment);

        const VkClearColorValue zeroClear = {{ 0.0f, 0.0f, 0.0f, 1.0f }};

        // Convert the clear color to the format of the attachment view
        Pal::ClearColor clearColor = VkToPalClearColor(
            (clear.isOptional == false) ?
                m_renderPassInstance.pAttachments[clear.attachment].clearValue.color : zeroClear,
            attachment.viewFormat);

        Pal::BoundColorTarget target = {};
        if (m_flags.subpassLoadOpClearsBoundAttachments)
        {
            const RenderPass* pRenderPass = m_allGpuState.pRenderPass;
            const uint32_t    subpass     = m_renderPassInstance.subpass;

            uint32_t          tgtIdx      = VK_ATTACHMENT_UNUSED;

            // Find color target of current attachment
            for (uint32_t colorTgt = 0; colorTgt < pRenderPass->GetSubpassColorReferenceCount(subpass); ++colorTgt)
            {
                const AttachmentReference& colorRef = pRenderPass->GetSubpassColorReference(subpass, colorTgt);
                if (clear.attachment == colorRef.attachment)
                {
                    tgtIdx = colorTgt;
                    break;
                }
            }
            VK_ASSERT(tgtIdx != VK_ATTACHMENT_UNUSED);

            target.targetIndex    = tgtIdx;
            target.swizzledFormat = attachment.viewFormat;
            target.samples        = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
            target.fragments      = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
            target.clearValue     = clearColor;
        }

        Pal::SubresRange subresRange;
        attachment.pView->GetFrameBufferAttachmentSubresRange(&subresRange);

        const Pal::ImageLayout clearLayout = RPGetAttachmentLayout(clear.attachment, subresRange.startSubres.plane);

        VK_ASSERT(clearLayout.usages & Pal::LayoutColorTarget);

        const auto clearSubresRanges = LoadOpClearSubresRanges(
            attachment, clear,
            *m_allGpuState.pRenderPass);

        utils::IterateMask deviceGroup(GetRpDeviceMask());

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::Box clearBox = BuildClearBox(m_renderPassInstance.renderArea[deviceIdx], attachment);

            if (m_flags.subpassLoadOpClearsBoundAttachments == false)
            {
                // Multi-RT clears are synchronized later in RPBeginSubpass()
                uint32 flags = 0;
                if (count == 1)
                {
                    flags |= Pal::ColorClearAutoSync;
                }
                if (clear.isOptional)
                {
                    flags |= Pal::ColorClearSkipIfSlow;
                }

                PalCmdBuffer(deviceIdx)->CmdClearColorImage(
                    *attachment.pImage->PalImage(deviceIdx),
                    clearLayout,
                    clearColor,
                    attachment.viewFormat,
                    clearSubresRanges.NumElements(),
                    clearSubresRanges.Data(),
                    1,
                    &clearBox,
                    flags);
            }
            else if (clear.isOptional == false) // Don't attempt optional bound clears yet
            {
                const RenderPass* pRenderPass = m_allGpuState.pRenderPass;
                const uint32_t    subpass     = m_renderPassInstance.subpass;
                uint32_t          viewMask    = pRenderPass->GetViewMask(subpass);

                const VkRect2D rect =
                {
                    { clearBox.offset.x,        clearBox.offset.y },            //    VkOffset2D    offset;
                    { clearBox.extent.width,    clearBox.extent.height},        //    VkExtent2D    extent;
                };

                const VkClearRect clearRect =
                {
                    rect,                                       // VkRect2D    rect;
                    static_cast<uint32_t>(clearBox.offset.z),   // deUint32    baseArrayLayer;
                    clearBox.extent.depth                       // deUint32    layerCount;
                };

                CreateClearRegions(
                    1,
                    &clearRect,
                    viewMask,
                    0u,
                    &clearRegions);

               // Clear the bound color targets
               // TODO: Batch color targets in one call
               PalCmdBuffer(deviceIdx)->CmdClearBoundColorTargets(
                   1,
                   &target,
                   clearRegions.NumElements(),
                   clearRegions.Data());
            }
        }
        while (deviceGroup.IterateNext());
    }

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->EndRenderPassColorClear();
    }
}

// =====================================================================================================================
// Does one or more load-op depth-stencil clears during a render pass instance.
void CmdBuffer::RPLoadOpClearDepthStencil(
    uint32_t                 count,
    const RPLoadOpClearInfo* pClears)
{
    if (m_pSqttState != nullptr)
    {
        m_pSqttState->BeginRenderPassDepthStencilClear();
    }

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    Util::Vector<Pal::ClearBoundTargetRegion, 8, VirtualStackFrame> clearRegions{ &virtStackFrame };

    const auto maxRects  = EstimateMaxObjectsOnVirtualStack(sizeof(VkClearRect));
    auto       rectBatch = Util::Min(count, maxRects);

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPLoadOpClearInfo& clear = pClears[i];

        const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(clear.attachment);

        const Pal::ImageLayout depthLayout   = RPGetAttachmentLayout(clear.attachment, 0);
        const Pal::ImageLayout stencilLayout = RPGetAttachmentLayout(clear.attachment, 1);

        // Convert the clear color to the format of the attachment view
        const VkClearValue& clearValue = m_renderPassInstance.pAttachments[clear.attachment].clearValue;

        float clearDepth        = VkToPalClearDepth(clearValue.depthStencil.depth);
        Pal::uint8 clearStencil = clearValue.depthStencil.stencil;

        const auto clearSubresRanges = LoadOpClearSubresRanges(
            attachment, clear,
            *m_allGpuState.pRenderPass);

        utils::IterateMask deviceGroup(GetRpDeviceMask());

        Pal::SubresRange subresRange;
        attachment.pView->GetFrameBufferAttachmentSubresRange(&subresRange);

        ValidateSamplePattern(
            attachment.pImage->GetImageSamples(),
            &m_renderPassInstance.pAttachments[clear.attachment].initialSamplePattern);

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            const Pal::Rect& palClearRect = m_renderPassInstance.renderArea[deviceIdx];

            if (m_flags.subpassLoadOpClearsBoundAttachments == false)
            {
                PalCmdBuffer(deviceIdx)->CmdClearDepthStencil(
                    *attachment.pImage->PalImage(deviceIdx),
                    depthLayout,
                    stencilLayout,
                    clearDepth,
                    clearStencil,
                    StencilWriteMaskFull,
                    clearSubresRanges.NumElements(),
                    clearSubresRanges.Data(),
                    1,
                    &palClearRect,
                    Pal::DsClearAutoSync);
            }
            else
            {
                const auto palResult = clearRegions.Reserve(rectBatch);

                VK_ASSERT(palResult == Pal::Result::Success);

                const RenderPass* pRenderPass = m_allGpuState.pRenderPass;
                const uint32_t    subpass     = m_renderPassInstance.subpass;
                uint32_t          viewMask    = pRenderPass->GetViewMask(subpass);

                // Get the corresponding color reference in the current subpass
                const AttachmentReference& depthStencilRef = pRenderPass->GetSubpassDepthStencilReference(subpass);

                VK_ASSERT(depthStencilRef.attachment != VK_ATTACHMENT_UNUSED);

                Pal::DepthStencilSelectFlags selectFlags = {};

                selectFlags.depth   = ((clear.aspect & VK_IMAGE_ASPECT_DEPTH_BIT  ) != 0);
                selectFlags.stencil = ((clear.aspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0);

                const VkRect2D rect =
                {
                    { palClearRect.offset.x,        palClearRect.offset.y },        //    VkOffset2D    offset;
                    { palClearRect.extent.width,    palClearRect.extent.height},    //    VkExtent2D    extent;
                };

                const VkClearRect clearRect =
                {
                    rect,                           // VkRect2D    rect;
                    0u,                             // deUint32    baseArrayLayer;
                    subresRange.numSlices           // deUint32    layerCount;
                };

                CreateClearRegions(
                    1,
                    &clearRect,
                    viewMask,
                    0u,
                    &clearRegions);

                // Clear the bound depth stencil target immediately
                PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundDepthStencilTargets(
                    clearDepth,
                    clearStencil,
                    StencilWriteMaskFull,
                    pRenderPass->GetDepthStencilAttachmentSamples(subpass),
                    pRenderPass->GetDepthStencilAttachmentSamples(subpass),
                    selectFlags,
                    clearRegions.NumElements(),
                    clearRegions.Data());
            }
        }
        while (deviceGroup.IterateNext());
    }

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->EndRenderPassDepthStencilClear();
    }
}

// =====================================================================================================================
// Launches one or more MSAA resolves during a render pass instance.
void CmdBuffer::RPResolveAttachments(
    uint32_t             count,
    const RPResolveInfo* pResolves)
{
    // Notify SQTT annotator that we are doing a render pass resolve operation
    if (m_pSqttState != nullptr)
    {
        m_pSqttState->BeginRenderPassResolve();
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPResolveInfo& params = pResolves[i];

        const Framebuffer::Attachment& srcAttachment =
            m_allGpuState.pFramebuffer->GetAttachment(params.src.attachment);
        const Framebuffer::Attachment& dstAttachment =
            m_allGpuState.pFramebuffer->GetAttachment(params.dst.attachment);

        // Both color and depth-stencil resolves are allowed by resolve attachments
        // SubresRange shall be exactly same for src and dst.
        VK_ASSERT(srcAttachment.subresRangeCount == dstAttachment.subresRangeCount);
        VK_ASSERT(srcAttachment.subresRange[0].numMips == 1);

        const uint32_t sliceCount = Util::Min(
            srcAttachment.subresRange[0].numSlices,
            dstAttachment.subresRange[0].numSlices);

        // We expect MSAA images to never have mipmaps
        VK_ASSERT(srcAttachment.subresRange[0].startSubres.mipLevel == 0);

        uint32_t aspectRegionCount                           = 0;
        uint32_t srcResolvePlanes[MaxRangePerAttachment]     = {};
        uint32_t dstResolvePlanes[MaxRangePerAttachment]     = {};
        const VkFormat   srcResolveFormat                    = srcAttachment.pView->GetViewFormat();
        const VkFormat   dstResolveFormat                    = dstAttachment.pView->GetViewFormat();
        Pal::ResolveMode resolveModes[MaxRangePerAttachment] = {};

        const Pal::MsaaQuadSamplePattern* pSampleLocations = nullptr;

        if (Formats::IsDepthStencilFormat(srcResolveFormat) == false)
        {
            resolveModes[0]     = Pal::ResolveMode::Average;
            srcResolvePlanes[0] = 0;
            dstResolvePlanes[0] = 0;
            aspectRegionCount   = 1;
        }
        else
        {
            const uint32_t subpass = m_renderPassInstance.subpass;

            const VkResolveModeFlagBits depthResolveMode   =
                m_allGpuState.pRenderPass->GetDepthResolveMode(subpass);
            const VkResolveModeFlagBits stencilResolveMode =
                m_allGpuState.pRenderPass->GetStencilResolveMode(subpass);
            const VkImageAspectFlags depthStecilAcpect     =
                m_allGpuState.pRenderPass->GetResolveDepthStecilAspect(subpass);

            if (Formats::HasDepth(srcResolveFormat))
            {
                // Must be specified because the source image was created with sampleLocsAlwaysKnown set
                pSampleLocations = &m_renderPassInstance.pSamplePatterns[subpass].locations;
            }

            if ((depthResolveMode != VK_RESOLVE_MODE_NONE) &&
                ((depthStecilAcpect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0))
            {
                VK_ASSERT(Formats::HasDepth(srcResolveFormat) && Formats::HasDepth(dstResolveFormat));

                resolveModes[aspectRegionCount]     = VkToPalResolveMode(depthResolveMode);
                srcResolvePlanes[aspectRegionCount] = 0;
                dstResolvePlanes[aspectRegionCount] = 0;
                aspectRegionCount++;
            }

            if ((stencilResolveMode != VK_RESOLVE_MODE_NONE) &&
                ((depthStecilAcpect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0))
            {
                VK_ASSERT(Formats::HasStencil(srcResolveFormat) && Formats::HasStencil(dstResolveFormat));

                resolveModes[aspectRegionCount]     = VkToPalResolveMode(stencilResolveMode);
                srcResolvePlanes[aspectRegionCount] = Formats::HasDepth(srcResolveFormat) ? 1 : 0;
                dstResolvePlanes[aspectRegionCount] = Formats::HasDepth(dstResolveFormat) ? 1 : 0;
                aspectRegionCount++;
            }
        }

        // Depth and stencil might have different resolve mode, so allowing resolve each aspect independently.
        for (uint32_t aspectRegionIndex = 0; aspectRegionIndex < aspectRegionCount; ++aspectRegionIndex)
        {
            // During split-frame-rendering, the image to resolve could be split across multiple devices.
            Pal::ImageResolveRegion regions[MaxPalDevices];

            const Pal::ImageLayout srcLayout = RPGetAttachmentLayout(params.src.attachment,
                                                                     srcResolvePlanes[aspectRegionIndex]);
            const Pal::ImageLayout dstLayout = RPGetAttachmentLayout(params.dst.attachment,
                                                                     dstResolvePlanes[aspectRegionIndex]);

            for (uint32_t idx = 0; idx < m_renderPassInstance.renderAreaCount; idx++)
            {
                const Pal::Rect& renderArea = m_renderPassInstance.renderArea[idx];

                regions[idx].srcPlane       = srcResolvePlanes[aspectRegionIndex];
                regions[idx].srcSlice       = srcAttachment.subresRange[0].startSubres.arraySlice;
                regions[idx].srcOffset.x    = renderArea.offset.x;
                regions[idx].srcOffset.y    = renderArea.offset.y;
                regions[idx].srcOffset.z    = 0;
                regions[idx].dstPlane       = dstResolvePlanes[aspectRegionIndex];
                regions[idx].dstMipLevel    = dstAttachment.subresRange[0].startSubres.mipLevel;
                regions[idx].dstSlice       = dstAttachment.subresRange[0].startSubres.arraySlice;
                regions[idx].dstOffset.x    = renderArea.offset.x;
                regions[idx].dstOffset.y    = renderArea.offset.y;
                regions[idx].dstOffset.z    = 0;
                regions[idx].extent.width   = renderArea.extent.width;
                regions[idx].extent.height  = renderArea.extent.height;
                regions[idx].extent.depth   = 1;
                regions[idx].numSlices      = sliceCount;
                regions[idx].swizzledFormat = Pal::UndefinedSwizzledFormat;

                regions[idx].pQuadSamplePattern = pSampleLocations;
            }

            PalCmdResolveImage(
                *srcAttachment.pImage,
                srcLayout,
                *dstAttachment.pImage,
                dstLayout,
                resolveModes[aspectRegionIndex],
                m_renderPassInstance.renderAreaCount,
                regions,
                GetRpDeviceMask());
        }
    }

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->EndRenderPassResolve();
    }
}

// =====================================================================================================================
// Binds color/depth targets for a subpass during a render pass instance.
void CmdBuffer::RPBindTargets(
    const RPBindTargetsInfo& targets)
{
    Pal::BindTargetParams params = {};

    params.colorTargetCount = targets.colorTargetCount;

    static constexpr Pal::ImageLayout NullLayout = {};

    utils::IterateMask deviceGroup(GetRpDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < targets.colorTargetCount; ++i)
        {
            const RPAttachmentReference& reference = targets.colorTargets[i];

            if (reference.attachment != VK_ATTACHMENT_UNUSED)
            {
                const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(reference.attachment);

                params.colorTargets[i].pColorTargetView = attachment.pView->PalColorTargetView(deviceIdx);
                params.colorTargets[i].imageLayout = RPGetAttachmentLayout(reference.attachment, 0);

            }
            else
            {
                params.colorTargets[i].pColorTargetView = nullptr;
                params.colorTargets[i].imageLayout = NullLayout;

            }
        }

        if (targets.depthStencil.attachment != VK_ATTACHMENT_UNUSED)
        {
            uint32_t attachmentIdx = targets.depthStencil.attachment;

            const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

            params.depthTarget.pDepthStencilView = attachment.pView->PalDepthStencilView(deviceIdx);
            params.depthTarget.depthLayout       = RPGetAttachmentLayout(attachmentIdx, 0);
            params.depthTarget.stencilLayout     = RPGetAttachmentLayout(attachmentIdx, 1);

        }
        else
        {
            params.depthTarget.pDepthStencilView = nullptr;
            params.depthTarget.depthLayout       = NullLayout;
            params.depthTarget.stencilLayout     = NullLayout;

        }

        PalCmdBuffer(deviceIdx)->CmdBindTargets(params);

        if (targets.fragmentShadingRateTarget.attachment != VK_ATTACHMENT_UNUSED)
        {
            uint32_t attachmentIdx = targets.fragmentShadingRateTarget.attachment;

            const Framebuffer::Attachment& attachment = m_allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

            PalCmdBuffer(deviceIdx)->CmdBindSampleRateImage(attachment.pImage->PalImage(deviceIdx));
        }

    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Get Pal Image aspect layout from imageView
void CmdBuffer::GetImageLayout(
    VkImageView        imageView,
    VkImageLayout      imageLayout,
    VkImageAspectFlags aspectMask,
    Pal::SubresRange*  palSubresRange,
    Pal::ImageLayout*  palImageLayout)
{
    // Get the image view from the attachment info
    const ImageView* const pImageView = ImageView::ObjectFromHandle(imageView);

    // Get the attachment image
    const Image* pImage = pImageView->GetImage();

    // Get subres range from the image view
    pImageView->GetFrameBufferAttachmentSubresRange(palSubresRange);

    palSubresRange->startSubres.plane = VkToPalImagePlaneSingle(
        pImage->GetFormat(),
        aspectMask,
        m_pDevice->GetRuntimeSettings());

    // Get the Depth Layout from the view image
    *palImageLayout = pImage->GetBarrierPolicy().GetAspectLayout(
        imageLayout,
        palSubresRange->startSubres.plane,
        GetQueueFamilyIndex(),
        pImage->GetFormat());
}

// =====================================================================================================================
// Binds color/depth targets for VK_KHR_dynamic_rendering
void CmdBuffer::BindTargets(
    const VkRenderingInfoKHR*                              pRenderingInfo,
    const VkRenderingFragmentShadingRateAttachmentInfoKHR* pRenderingFragmentShadingRateAttachmentInfoKHR)
{
    Pal::BindTargetParams params = {};

    params.colorTargetCount = pRenderingInfo->colorAttachmentCount;

    static constexpr Pal::ImageLayout NullLayout = {};

    utils::IterateMask deviceGroup(GetDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < params.colorTargetCount; ++i)
        {
            const VkRenderingAttachmentInfoKHR& renderingAttachmentInfo = pRenderingInfo->pColorAttachments[i];

            if (renderingAttachmentInfo.imageView != VK_NULL_HANDLE)
            {
                // Get the image view from the attachment info
                const ImageView* const pImageView = ImageView::ObjectFromHandle(renderingAttachmentInfo.imageView);

                // Get the attachment image
                const Image* pImage = pImageView->GetImage();

                params.colorTargets[i].pColorTargetView = pImageView->PalColorTargetView(deviceIdx);

                RPImageLayout imageLayout =
                {
                    renderingAttachmentInfo.imageLayout,
                    0
                };

                params.colorTargets[i].imageLayout =
                    pImage->GetAttachmentLayout(
                        imageLayout,
                        0,
                        this);

            }
            else
            {
                params.colorTargets[i].pColorTargetView = nullptr;
                params.colorTargets[i].imageLayout = NullLayout;

            }
        }

        const VkRenderingAttachmentInfoKHR* pStencilAttachmentInfo = pRenderingInfo->pStencilAttachment;

        if ((pStencilAttachmentInfo != nullptr) &&
            (pStencilAttachmentInfo->imageView != VK_NULL_HANDLE))
        {
            const ImageView* const pStencilImageView =
                ImageView::ObjectFromHandle(pStencilAttachmentInfo->imageView);

            Pal::SubresRange subresRange = {};
            Pal::ImageLayout stencilLayout = {};

            GetImageLayout(
                pStencilAttachmentInfo->imageView,
                pStencilAttachmentInfo->imageLayout,
                VK_IMAGE_ASPECT_STENCIL_BIT,
                &subresRange,
                &stencilLayout);

            params.depthTarget.pDepthStencilView = pStencilImageView->PalDepthStencilView(deviceIdx);
            params.depthTarget.stencilLayout = stencilLayout;
        }
        else
        {
            params.depthTarget.pDepthStencilView = nullptr;
            params.depthTarget.stencilLayout     = NullLayout;
        }

        const VkRenderingAttachmentInfoKHR* pDepthAttachmentInfo = pRenderingInfo->pDepthAttachment;

        if ((pDepthAttachmentInfo != nullptr) &&
            (pDepthAttachmentInfo->imageView != VK_NULL_HANDLE))
        {
            const ImageView* const pDepthImageView =
                ImageView::ObjectFromHandle(pDepthAttachmentInfo->imageView);

            Pal::SubresRange subresRange = {};
            Pal::ImageLayout depthLayout = {};

            GetImageLayout(
                pDepthAttachmentInfo->imageView,
                pDepthAttachmentInfo->imageLayout,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                &subresRange,
                &depthLayout);

            params.depthTarget.pDepthStencilView = pDepthImageView->PalDepthStencilView(deviceIdx);
            params.depthTarget.depthLayout       = depthLayout;
        }
        else
        {
            // Set the depthLayout for stencil only formats to avoid incorrect PAL asserts.
            params.depthTarget.depthLayout       = params.depthTarget.stencilLayout;
        }

        PalCmdBuffer(deviceIdx)->CmdBindTargets(params);

        if ((pRenderingFragmentShadingRateAttachmentInfoKHR != nullptr) &&
            (pRenderingFragmentShadingRateAttachmentInfoKHR->imageView != VK_NULL_HANDLE))
        {
            // Get the image view from the attachment info
            const ImageView* const pImageView =
                ImageView::ObjectFromHandle(pRenderingFragmentShadingRateAttachmentInfoKHR->imageView);

            // Get the attachment image
            const Image* pImage = pImageView->GetImage();

            PalCmdBuffer(deviceIdx)->CmdBindSampleRateImage(pImage->PalImage(deviceIdx));
        }

    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Sets view instance mask for a subpass during a render pass instance (on devices within passed in device mask).
void CmdBuffer::SetViewInstanceMask(
    uint32_t deviceMask)
{
    uint32_t subpassViewMask = 0;

    if (m_allGpuState.pRenderPass != nullptr)
    {
        subpassViewMask = m_allGpuState.pRenderPass->GetViewMask(m_renderPassInstance.subpass);
    }
    else if (m_allGpuState.dynamicRenderingInstance.viewMask > 0)
    {
        subpassViewMask = m_allGpuState.dynamicRenderingInstance.viewMask;
    }

    utils::IterateMask deviceGroup(deviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        const uint32_t deviceViewMask = uint32_t { 0x1 } << deviceIdx;

        uint32_t viewMask = 0x0;

        if (m_allGpuState.viewIndexFromDeviceIndex)
        {
            // VK_KHR_multiview interaction with VK_KHR_device_group.
            // When GraphicsPipeline is created with flag
            // VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
            // rendering to views is split across multiple devices.
            // Essentially this flag allows application to divide work
            // between devices when multiview rendering is enabled.
            // Basically each device renders one view.

            // Vulkan Spec: VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
            // specifies that any shader input variables decorated as DeviceIndex
            // will be assigned values as if they were decorated as ViewIndex.
            // To satisfy above requirement DeviceMask and ViewMask has to match.
            VK_ASSERT(m_curDeviceMask == viewMask);

            // Currently Vulkan CTS lacks tests covering this functionality.
            VK_NOT_TESTED();

            viewMask = deviceViewMask;
        }
        else
        {
            // In default mode work is duplicated on each device,
            // because the same viewMask is set for all devices.
            // Basically each device renders all views.
            viewMask = subpassViewMask;
        }

        PalCmdBuffer(deviceIdx)->CmdSetViewInstanceMask(viewMask);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Ends a render pass instance (vkCmdEndRenderPass)
void CmdBuffer::EndRenderPass()
{
    DbgBarrierPreCmd(DbgBarrierEndRenderPass);

    if (m_renderPassInstance.subpass != VK_SUBPASS_EXTERNAL)
    {
        // Close the previous subpass
        RPEndSubpass();

        // Get the end state for this render pass instance
        const RPExecuteEndRenderPassInfo& end = m_allGpuState.pRenderPass->GetExecuteInfo()->end;

        // Synchronize any prior work before leaving the instance (external dependencies) and also handle final layout
        // transitions.
        if (end.syncEnd.flags.active)
        {
            VirtualStackFrame virtStack(m_pStackAllocator);

            RPSyncPoint(end.syncEnd, &virtStack);
        }
    }

    // Clean up instance state
    m_allGpuState.pRenderPass   = nullptr;
    m_allGpuState.pFramebuffer  = nullptr;
    m_renderPassInstance.pExecuteInfo = nullptr;

    DbgBarrierPostCmd(DbgBarrierEndRenderPass);
}

// =====================================================================================================================
void CmdBuffer::WritePushConstants(
    PipelineBindPoint      apiBindPoint,
    Pal::PipelineBindPoint palBindPoint,
    const PipelineLayout*  pLayout,
    uint32_t               startInDwords,
    uint32_t               lengthInDwords,
    const uint32_t* const  pInputValues)
{
    PipelineBindState* pBindState = &m_allGpuState.pipelineState[apiBindPoint];
    Pal::uint32* pUserData = reinterpret_cast<Pal::uint32*>(&pBindState->pushConstData[0]);
    uint32_t* pUserDataPtr = pUserData + startInDwords;

    for (uint32_t i = 0; i < lengthInDwords; i++)
    {
        pUserDataPtr[i] = pInputValues[i];
    }

    pBindState->pushedConstCount = Util::Max(pBindState->pushedConstCount, startInDwords + lengthInDwords);

    const UserDataLayout& userDataLayout = pLayout->GetInfo().userDataLayout;

    if (userDataLayout.scheme == PipelineLayoutScheme::Compact)
    {
        // Program the user data register only if the current user data layout base matches that of the given
        // layout.  Otherwise, what's happening is that the application is pushing constants for a future
        // pipeline layout (e.g. at the top of the command buffer) and this register write will be redundant because
        // a future vkCmdBindPipeline will reprogram the user data registers during the rebase.
        if (PalPipelineBindingOwnedBy(palBindPoint, apiBindPoint) &&
            (pBindState->userDataLayout.compact.pushConstRegBase == userDataLayout.compact.pushConstRegBase) &&
            (pBindState->userDataLayout.compact.pushConstRegCount >= (startInDwords + lengthInDwords)))
        {
            utils::IterateMask deviceGroup(m_curDeviceMask);
            do
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdSetUserData(
                    palBindPoint,
                    pBindState->userDataLayout.compact.pushConstRegBase + startInDwords,
                    lengthInDwords,
                    pUserDataPtr);
            }
            while (deviceGroup.IterateNext());
        }
    }
    else if (userDataLayout.scheme == PipelineLayoutScheme::Indirect)
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::gpusize gpuAddr;

            void* pCpuAddr = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(
                userDataLayout.indirect.pushConstSizeInDword,
                m_pDevice->GetProperties().descriptorSizes.alignmentInDwords,
                &gpuAddr);

            memcpy(pCpuAddr, pUserData, userDataLayout.indirect.pushConstSizeInDword * sizeof(uint32_t));

            const uint32_t gpuAddrLow = static_cast<uint32_t>(gpuAddr);

            PalCmdBuffer(deviceIdx)->CmdSetUserData(
                palBindPoint,
                userDataLayout.indirect.pushConstPtrRegBase,
                PipelineLayout::SetPtrRegCount,
                &gpuAddrLow);
        }
        while (deviceGroup.IterateNext());
    }
    else
    {
        VK_NEVER_CALLED();
    }
}

// =====================================================================================================================
// Set push constant values
void CmdBuffer::PushConstants(
    VkPipelineLayout                            layout,
    VkShaderStageFlags                          stageFlags,
    uint32_t                                    start,
    uint32_t                                    length,
    const void*                                 values)
{
    DbgBarrierPreCmd(DbgBarrierBindSetsPushConstants);

    uint32_t startInDwords  = start / sizeof(uint32_t);
    uint32_t lengthInDwords = length / sizeof(uint32_t);

    const uint32_t* const pInputValues = reinterpret_cast<const uint32_t*>(values);

    const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(layout);

    stageFlags &= m_validShaderStageFlags;

    PushConstantsIssueWrites(pLayout, stageFlags, startInDwords, lengthInDwords, pInputValues);

    DbgBarrierPostCmd(DbgBarrierBindSetsPushConstants);
}

// =====================================================================================================================
void CmdBuffer::PushConstantsIssueWrites(
    const PipelineLayout*  pLayout,
    VkShaderStageFlags     stageFlags,
    uint32_t               startInDwords,
    uint32_t               lengthInDwords,
    const uint32_t* const  pInputValues)
{
    if ((stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) != 0)
    {
        WritePushConstants(PipelineBindCompute,
                           Pal::PipelineBindPoint::Compute,
                           pLayout,
                           startInDwords,
                           lengthInDwords,
                           pInputValues);

    }

#if VKI_RAY_TRACING
    if ((stageFlags & RayTraceShaderStages) != 0)
    {
        WritePushConstants(PipelineBindRayTracing,
                           Pal::PipelineBindPoint::Compute,
                           pLayout,
                           startInDwords,
                           lengthInDwords,
                           pInputValues);
    }
#endif

    if ((stageFlags & ShaderStageAllGraphics) != 0)
    {
        WritePushConstants(PipelineBindGraphics,
                           Pal::PipelineBindPoint::Graphics,
                           pLayout,
                           startInDwords,
                           lengthInDwords,
                           pInputValues);
    }
}

// =====================================================================================================================
// Creates or grows an internal descriptor set for the command buffer to push
template <uint32_t numPalDevices>
VkDescriptorSet CmdBuffer::InitPushDescriptorSet(
    const DescriptorSetLayout*               pDestSetLayout,
    const PipelineLayout::SetUserDataLayout& setLayoutInfo,
    const size_t                             descriptorSetSize,
    PipelineBindPoint                        bindPoint,
    const uint32_t                           alignmentInDwords)
{
    // The descriptor writes must go to the command buffer's shadow to handle incremental updates.
    // Any used descriptors are required to be pushed before the pipeline is executed or else they are undefined,
    // which means the last push descriptor set's value or uninitialized memory because no special care is taken here.
    DescriptorSet<numPalDevices>* pSet = DescriptorSet<numPalDevices>::ObjectFromHandle(
            m_allGpuState.pipelineState[bindPoint].pushDescriptorSet);

    // Reuse the existing shadow buffer unless it wasn't created or needs to grow.
    if (descriptorSetSize > m_allGpuState.pipelineState[bindPoint].pushDescriptorSetMaxSize)
    {
        const size_t objSize = Util::Pow2Align(sizeof(DescriptorSet<numPalDevices>), VK_DEFAULT_MEM_ALIGN);

        // Note that descriptor sets don't require a destructor to be called
        m_pDevice->VkInstance()->FreeMem(m_allGpuState.pipelineState[bindPoint].pPushDescriptorSetMemory);

        void* pSetMem = m_pDevice->VkInstance()->AllocMem(
            (descriptorSetSize * numPalDevices) + objSize,
            (alignmentInDwords * sizeof(uint32_t)),
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSetMem != nullptr)
        {
            pSet = VK_PLACEMENT_NEW (Util::VoidPtrInc(pSetMem, (descriptorSetSize * numPalDevices)))
                DescriptorSet<numPalDevices>(0);

            // Store the API handle to avoid templated parameters when using it.
            m_allGpuState.pipelineState[bindPoint].pushDescriptorSet        =
                DescriptorSet<numPalDevices>::HandleFromObject(pSet);
            m_allGpuState.pipelineState[bindPoint].pPushDescriptorSetMemory = pSetMem;
            m_allGpuState.pipelineState[bindPoint].pushDescriptorSetMaxSize = descriptorSetSize;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
            pSet = nullptr;

            m_allGpuState.pipelineState[bindPoint].pushDescriptorSet        = VK_NULL_HANDLE;
            m_allGpuState.pipelineState[bindPoint].pPushDescriptorSetMemory = nullptr;
            m_allGpuState.pipelineState[bindPoint].pushDescriptorSetMaxSize = 0;
        }
    }

    if (pSet != nullptr)
    {
        DescriptorAddr descriptorAddrs[numPalDevices] = {};

        // If there is a set pointer, the shadow memory is that of the push descriptor set. Otherwise, the descriptor
        // set is written inline to the command buffer binding data set shadow memory.
        if (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg)
        {
            for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
            {
                descriptorAddrs[deviceIdx].staticCpuAddr =
                    static_cast<uint32_t*>(Util::VoidPtrInc(
                        m_allGpuState.pipelineState[bindPoint].pPushDescriptorSetMemory, descriptorSetSize * deviceIdx));
            }
        }
        else
        {
            for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
            {
                descriptorAddrs[deviceIdx].staticCpuAddr =
                    &(PerGpuState(deviceIdx)->setBindingData[bindPoint][setLayoutInfo.firstRegOffset]);
            }
        }

        pSet->Reassign(pDestSetLayout, 0, descriptorAddrs, nullptr);
    }

    // Push descriptor sets don't use vkAllocateDescriptorSets, so if they must be written to the descriptor set,
    // every push of an immutable sampler must be honored instead of skipping as we do today. Write them all here
    // until it's known if not skipping them must be implemented.
    if (m_pDevice->MustWriteImmutableSamplers())
    {
        VK_NOT_IMPLEMENTED;

        pSet->WriteImmutableSamplers(m_pDevice->GetProperties().descriptorSizes.imageView);
    }

    return DescriptorSet<numPalDevices>::HandleFromObject(pSet);
}

// =====================================================================================================================
template <size_t imageDescSize,
          size_t samplerDescSize,
          size_t bufferDescSize,
          uint32_t numPalDevices>
void CmdBuffer::PushDescriptorSetKHR(
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites)
{
    DbgBarrierPreCmd(DbgBarrierPushDescriptorSet);

    const PipelineLayout*                    pLayout         = PipelineLayout::ObjectFromHandle(layout);
    const DescriptorSetLayout*               pDestSetLayout  = pLayout->GetSetLayouts(set);
    const PipelineLayout::SetUserDataLayout& setLayoutInfo   = pLayout->GetSetUserData(set);
    const uint8                              setPtrRegOffset = setLayoutInfo.setPtrRegOffset;

    Pal::PipelineBindPoint palBindPoint;
    PipelineBindPoint      apiBindPoint;

    ConvertPipelineBindPoint(pipelineBindPoint, &palBindPoint, &apiBindPoint);

    const uint32_t descriptorSetSizeInDwords = pDestSetLayout->Info().sta.dwSize;
    const uint32_t alignmentInDwords         = m_pDevice->GetProperties().descriptorSizes.alignmentInDwords;

    // An internal descriptor set is used to represent the shadow to be consistent with the
    // vkCmdPushDescriptorSetWithTemplateKHR implementation only. WriteDescriptorSets would have to have
    // been modified to accept the destination set as a new parameter instead of using VkWriteDescriptorSet.
    VkDescriptorSet pushDescriptorSet = InitPushDescriptorSet<numPalDevices>(
        pDestSetLayout,
        setLayoutInfo,
        (descriptorSetSizeInDwords * sizeof(uint32_t)),
        apiBindPoint,
        alignmentInDwords);

    DescriptorSet<numPalDevices>* pDestSet = DescriptorSet<numPalDevices>::ObjectFromHandle(pushDescriptorSet);

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // Issue the descriptor writes using the destination address of the command buffer's shadow rather than the
        // descriptor set memory; the dstSet member of VkWriteDescriptorSet must be ignored for push descriptors.
        for (uint32_t i = 0; i < descriptorWriteCount; ++i)
        {
            const VkWriteDescriptorSet&             params      = pDescriptorWrites[i];
            const DescriptorSetLayout::BindingInfo& destBinding = pDestSetLayout->Binding(params.dstBinding);

            uint32_t* pDestAddr = pDestSet->StaticCpuAddress(deviceIdx) +
                                  pDestSetLayout->GetDstStaOffset(destBinding, params.dstArrayElement);

            // Determine whether the binding has immutable sampler descriptors.
            const bool hasImmutableSampler = (destBinding.imm.dwSize != 0);

            switch (static_cast<uint32_t>(params.descriptorType))
            {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                if (hasImmutableSampler == false)
                {
                    DescriptorUpdate::WriteSamplerDescriptors<samplerDescSize>(
                        params.pImageInfo,
                        pDestAddr,
                        params.descriptorCount,
                        destBinding.sta.dwArrayStride);
                }
                break;

            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                if (hasImmutableSampler)
                {
                    if (destBinding.bindingFlags.ycbcrConversionUsage == 0)
                    {
                        // If the sampler part of the combined image sampler is immutable then we should only update
                        // the image descriptors, but have to make sure to still use the appropriate stride.
                        DescriptorUpdate::WriteImageDescriptors<imageDescSize, false>(
                            params.pImageInfo,
                            deviceIdx,
                            pDestAddr,
                            params.descriptorCount,
                            destBinding.sta.dwArrayStride);
                    }
                    else
                    {
                        DescriptorUpdate::WriteImageDescriptorsYcbcr<imageDescSize>(
                            params.pImageInfo,
                            deviceIdx,
                            pDestAddr,
                            params.descriptorCount,
                            destBinding.sta.dwArrayStride);
                    }
                }
                else
                {
                    DescriptorUpdate::WriteImageSamplerDescriptors<imageDescSize, samplerDescSize>(
                        params.pImageInfo,
                        deviceIdx,
                        pDestAddr,
                        params.descriptorCount,
                        destBinding.sta.dwArrayStride);
                }
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                DescriptorUpdate::WriteImageDescriptors<imageDescSize, true>(
                    params.pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                DescriptorUpdate::WriteImageDescriptors<imageDescSize, false>(
                    params.pImageInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                DescriptorUpdate::WriteBufferDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER>(
                    params.pTexelBufferView,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                DescriptorUpdate::WriteBufferDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER>(
                    params.pTexelBufferView,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                DescriptorUpdate::WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER>(
                    m_pDevice,
                    params.pBufferInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                DescriptorUpdate::WriteBufferInfoDescriptors<bufferDescSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER>(
                    m_pDevice,
                    params.pBufferInfo,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);
                break;

#if VKI_RAY_TRACING
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            {
                const auto* pWriteAccelStructKHR =
                    reinterpret_cast<const VkWriteDescriptorSetAccelerationStructureKHR*>(
                    utils::GetExtensionStructure(reinterpret_cast<const VkStructHeader*>(params.pNext),
                    static_cast<VkStructureType>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR)));

                VK_ASSERT(pWriteAccelStructKHR != nullptr);
                VK_ASSERT(pWriteAccelStructKHR->accelerationStructureCount == params.descriptorCount);

                DescriptorUpdate::WriteAccelerationStructureDescriptors(
                    m_pDevice,
                    pWriteAccelStructKHR->pAccelerationStructures,
                    deviceIdx,
                    pDestAddr,
                    params.descriptorCount,
                    destBinding.sta.dwArrayStride);

                break;
            }
#endif

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT:
            default:
                VK_ASSERT(!"Unexpected descriptor type");
                break;
            }
        }

        // If there is a set pointer, update the push descriptor set from the command buffer shadow set to an embedded
        // memory allocation. Otherwise, the shadow set contents will be directly written to user data instead of this
        // push descriptor set pointer.
        if (setPtrRegOffset != PipelineLayout::InvalidReg)
        {
            Pal::gpusize gpuAddr;
            uint32*      pCpuAddr = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(descriptorSetSizeInDwords,
                                                                                     alignmentInDwords,
                                                                                     &gpuAddr);

            memcpy(pCpuAddr, pDestSet->StaticCpuAddress(deviceIdx), (descriptorSetSizeInDwords * sizeof(uint32_t)));

            // CmdAllocateEmbeddedData is allocated out of VaRange::DescriptorTable, so the upper half is
            // known by the shader as is the case for our descriptor pool allocations.
            PerGpuState(deviceIdx)->setBindingData[apiBindPoint][setPtrRegOffset] = static_cast<uint32_t>(gpuAddr);
        }

        SetUserDataPipelineLayout(set, 1, pLayout, palBindPoint, apiBindPoint);
    }
    while (deviceGroup.IterateNext());

    DbgBarrierPostCmd(DbgBarrierPushDescriptorSet);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
void CmdBuffer::PushDescriptorSetWithTemplateKHR(
    VkDescriptorUpdateTemplate                  descriptorUpdateTemplate,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    const void*                                 pData)
{
    DbgBarrierPreCmd(DbgBarrierPushDescriptorSet);

    const PipelineLayout*      pLayout        = PipelineLayout::ObjectFromHandle(layout);
    const DescriptorSetLayout* pDestSetLayout = pLayout->GetSetLayouts(set);
    DescriptorUpdateTemplate*  pTemplate      = DescriptorUpdateTemplate::ObjectFromHandle(descriptorUpdateTemplate);

    Pal::PipelineBindPoint palBindPoint;
    PipelineBindPoint      apiBindPoint;

    ConvertPipelineBindPoint(pTemplate->GetPipelineBindPoint(), &palBindPoint, &apiBindPoint);

    const uint32_t descriptorSetSizeInDwords = pDestSetLayout->Info().sta.dwSize;
    const uint32_t alignmentInDwords         = m_pDevice->GetProperties().descriptorSizes.alignmentInDwords;

    const PipelineLayout::SetUserDataLayout& setLayoutInfo = pLayout->GetSetUserData(set);

    // An internal descriptor set is used to represent the shadow to utilize normal descriptor write support
    // for updating the shadow. Push descriptors can be represented by only the static section of the descriptor set
    // layout because not all descriptor types are supported.
    VkDescriptorSet pushDescriptorSet = InitPushDescriptorSet<numPalDevices>(
        pDestSetLayout,
        setLayoutInfo,
        (descriptorSetSizeInDwords * sizeof(uint32_t)),
        apiBindPoint,
        alignmentInDwords);

    // Issue the descriptor template update using the internal descriptor set to use the destination address of the
    // command buffer's shadow rather than descriptor pool memory like regular descriptor sets.
    pTemplate->Update(
        m_pDevice,
        pushDescriptorSet,
        pData);

    const uint8 setPtrRegOffset = setLayoutInfo.setPtrRegOffset;

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // If there is a set pointer, update the push descriptor set from the command buffer shadow set to an embedded
        // memory allocation. Otherwise, the shadow set contents will be directly written to user data instead of this
        // push descriptor set pointer.
        if (setPtrRegOffset != PipelineLayout::InvalidReg)
        {
            Pal::gpusize gpuAddr;
            uint32*      pCpuAddr = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(descriptorSetSizeInDwords,
                                                                                     alignmentInDwords,
                                                                                     &gpuAddr);

            const DescriptorSet<numPalDevices>* pShadowSet =
                DescriptorSet<numPalDevices>::ObjectFromHandle(pushDescriptorSet);

            memcpy(pCpuAddr, pShadowSet->StaticCpuAddress(deviceIdx), (descriptorSetSizeInDwords * sizeof(uint32_t)));

            // CmdAllocateEmbeddedData is allocated out of VaRange::DescriptorTable, so the upper half is
            // known by the shader as is the case for our descriptor pool allocations.
            PerGpuState(deviceIdx)->setBindingData[apiBindPoint][setPtrRegOffset] = static_cast<uint32_t>(gpuAddr);
        }

        SetUserDataPipelineLayout(set, 1, pLayout, palBindPoint, apiBindPoint);
    }
    while (deviceGroup.IterateNext());

    DbgBarrierPostCmd(DbgBarrierPushDescriptorSet);
}

// =====================================================================================================================
void CmdBuffer::SetViewport(
    uint32_t            firstViewport,
    uint32_t            viewportCount,
    const VkViewport*   pViewports)
{
    // If we hit this assert the application did not set the right number of viewports
    // in VkPipelineViewportStateCreateInfo.viewportCount.
    // VK_ASSERT((firstViewport + viewportCount) <= m_state.viewport.count);

    const bool khrMaintenance1 = ((m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion() >= VK_MAKE_API_VERSION( 0, 1, 1, 0)) ||
                                  m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_MAINTENANCE1));

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIndex = deviceGroup.Index();

        for (uint32_t i = 0; i < viewportCount; ++i)
        {
            VkToPalViewport(pViewports[i],
                            firstViewport + i,
                            khrMaintenance1,
                            &PerGpuState(deviceIndex)->viewport);
        }
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.dirtyGraphics.viewport         = 1;
    m_allGpuState.staticTokens.viewports = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetViewportWithCount(
    uint32_t            viewportCount,
    const VkViewport*   pViewports)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PerGpuState(deviceGroup.Index())->viewport.count = viewportCount;
    }
    while (deviceGroup.IterateNext());

    SetViewport(0, viewportCount, pViewports);
}

// =====================================================================================================================
void CmdBuffer::SetAllViewports(
    const Pal::ViewportParams& params,
    uint32_t                   staticToken)
{
    VK_ASSERT(m_cbBeginDeviceMask == m_pDevice->GetPalDeviceMask());
    utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < params.count; ++i)
        {
            PerGpuState(deviceIdx)->viewport.viewports[i] = params.viewports[i];
        }

        PerGpuState(deviceIdx)->viewport.count = params.count;
        PerGpuState(deviceIdx)->viewport.depthRange = params.depthRange;
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.dirtyGraphics.viewport         = 1;
    m_allGpuState.staticTokens.viewports = staticToken;
}

// =====================================================================================================================
void CmdBuffer::SetScissor(
    uint32_t            firstScissor,
    uint32_t            scissorCount,
    const VkRect2D*     pScissors)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < scissorCount; ++i)
        {
            VkToPalScissorRect(pScissors[i], firstScissor + i, &PerGpuState(deviceIdx)->scissor);
        }
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.dirtyGraphics.scissor            = 1;
    m_allGpuState.staticTokens.scissorRect = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetScissorWithCount(
    uint32_t            scissorCount,
    const VkRect2D*     pScissors)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PerGpuState(deviceGroup.Index())->scissor.count = scissorCount;
    }
    while (deviceGroup.IterateNext());

    SetScissor(0, scissorCount, pScissors);
}

// =====================================================================================================================
void CmdBuffer::SetAllScissors(
    const Pal::ScissorRectParams& params,
    uint32_t                      staticToken)
{
    VK_ASSERT(m_cbBeginDeviceMask == m_pDevice->GetPalDeviceMask());

    utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PerGpuState(deviceIdx)->scissor.count = params.count;

        for (uint32_t i = 0; i < params.count; ++i)
        {
            PerGpuState(deviceIdx)->scissor.scissors[i] = params.scissors[i];
        }
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.dirtyGraphics.scissor            = 1;
    m_allGpuState.staticTokens.scissorRect = staticToken;
}

// =====================================================================================================================
void CmdBuffer::SetLineWidth(
    float               lineWidth)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const VkPhysicalDeviceLimits& limits = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetLimits();

    const Pal::PointLineRasterStateParams params = { DefaultPointSize,
                                                     lineWidth,
                                                     limits.pointSizeRange[0],
                                                     limits.pointSizeRange[1] };

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetPointLineRasterState(params);
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.staticTokens.pointLineRasterState = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetDepthBias(
    float               depthBias,
    float               depthBiasClamp,
    float               slopeScaledDepthBias)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const Pal::DepthBiasParams params = {depthBias, depthBiasClamp, slopeScaledDepthBias};

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetDepthBiasState(params);
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.staticTokens.depthBiasState = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetBlendConstants(
    const float         blendConst[4])
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const Pal::BlendConstParams params = { blendConst[0], blendConst[1], blendConst[2], blendConst[3] };

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetBlendConst(params);
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.staticTokens.blendConst = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetDepthBounds(
    float               minDepthBounds,
    float               maxDepthBounds)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const Pal::DepthBoundsParams params = { minDepthBounds, maxDepthBounds };

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetDepthBounds(params);
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.staticTokens.depthBounds = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetStencilCompareMask(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilCompareMask)
{
    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_allGpuState.stencilRefMasks.frontReadMask = static_cast<uint8_t>(stencilCompareMask);
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_allGpuState.stencilRefMasks.backReadMask = static_cast<uint8_t>(stencilCompareMask);
    }

    m_allGpuState.dirtyGraphics.stencilRef = 1;
}

// =====================================================================================================================
void CmdBuffer::SetStencilWriteMask(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilWriteMask)
{
    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_allGpuState.stencilRefMasks.frontWriteMask = static_cast<uint8_t>(stencilWriteMask);
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_allGpuState.stencilRefMasks.backWriteMask = static_cast<uint8_t>(stencilWriteMask);
    }

    m_allGpuState.dirtyGraphics.stencilRef = 1;
}

// =====================================================================================================================
void CmdBuffer::SetStencilReference(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilReference)
{
    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_allGpuState.stencilRefMasks.frontRef = static_cast<uint8_t>(stencilReference);
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_allGpuState.stencilRefMasks.backRef = static_cast<uint8_t>(stencilReference);
    }

    m_allGpuState.dirtyGraphics.stencilRef = 1;
}

// =====================================================================================================================
// Calculate the hash of dynamic vertex input info
static uint64_t GetDynamicVertexInputHash(
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
    Util::MetroHash::Hash hash = {};
    if (vertexBindingDescriptionCount > 0)
    {
        VK_ASSERT(vertexAttributeDescriptionCount > 0);
        Util::MetroHash64 hasher;
        hasher.Update(reinterpret_cast<const uint8_t*>(pVertexBindingDescriptions),
            sizeof(VkVertexInputBindingDescription2EXT) * vertexBindingDescriptionCount);
        hasher.Update(reinterpret_cast<const uint8_t*>(pVertexAttributeDescriptions),
            sizeof(VkVertexInputAttributeDescription2EXT) * vertexAttributeDescriptionCount);
        hasher.Finalize(hash.bytes);
    }
    return hash.qwords[0];
}

// =====================================================================================================================
// Builds uber-fetch shader internal data according to dynamic vertex input info.
DynamicVertexInputInternalData* CmdBuffer::BuildUberFetchShaderInternalData(
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
    uint64_t vertexInputHash = GetDynamicVertexInputHash(
        vertexBindingDescriptionCount,
        pVertexBindingDescriptions,
        vertexAttributeDescriptionCount,
        pVertexAttributeDescriptions);

    DynamicVertexInputInternalData* pVertexInputData = nullptr;

    bool         existed = false;
    Util::Result result  = m_uberFetchShaderInternalDataMap.FindAllocate(vertexInputHash, &existed, &pVertexInputData);
    if (result == Util::Result::Success)
    {
        if (existed == false)
        {
            if (m_pUberFetchShaderTempBuffer == nullptr)
            {
                m_pUberFetchShaderTempBuffer = m_pDevice->VkInstance()->AllocMem(
                    PipelineCompiler::GetMaxUberFetchShaderInternalDataSize() * NumPalDevices(),
                    VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
            }

            if (m_pUberFetchShaderTempBuffer != nullptr)
            {
                void* pUberFetchShaderInternalData = m_pUberFetchShaderTempBuffer;
                utils::IterateMask deviceGroup(m_curDeviceMask);
                do
                {
                    const uint32_t deviceIdx = deviceGroup.Index();

                    uint32_t uberFetchShaderInternalDataSize =
                        m_pDevice->GetCompiler(deviceIdx)->BuildUberFetchShaderInternalData(
                            vertexBindingDescriptionCount,
                            pVertexBindingDescriptions,
                            vertexAttributeDescriptionCount,
                            pVertexAttributeDescriptions,
                            pUberFetchShaderInternalData);

                    Pal::gpusize gpuAddress = {};
                    if (uberFetchShaderInternalDataSize > 0)
                    {
                        void* pCpuAddr = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(
                            uberFetchShaderInternalDataSize, 1, &gpuAddress);
                        memcpy(pCpuAddr, pUberFetchShaderInternalData, uberFetchShaderInternalDataSize);
                    }
                    pVertexInputData->gpuAddress[deviceIdx] = gpuAddress;

                    pUberFetchShaderInternalData =
                        Util::VoidPtrInc(pUberFetchShaderInternalData, uberFetchShaderInternalDataSize);
                } while (deviceGroup.IterateNext());

                // we needn't set any user data if internal size is 0.
                if (pVertexInputData->gpuAddress[0] == 0)
                {
                    pVertexInputData = nullptr;
                }
            }
            else
            {
                // return nullptr for any fail case.
                VK_NEVER_CALLED();
                pVertexInputData = nullptr;
            }
        }
    }
    else
    {
        VK_NEVER_CALLED();
        pVertexInputData = nullptr;
    }

    return pVertexInputData;
}

// =====================================================================================================================
void CmdBuffer::SetVertexInput(
    uint32_t                                     vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT*   pVertexBindingDescriptions,
    uint32_t                                     vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT* pVertexAttributeDescriptions)
{
    PipelineBindState* pBindState = &m_allGpuState.pipelineState[PipelineBindGraphics];
    const bool padVertexBuffers = m_flags.padVertexBuffers;

    pBindState->pVertexInputInternalData = BuildUberFetchShaderInternalData(
        vertexBindingDescriptionCount,
        pVertexBindingDescriptions,
        vertexAttributeDescriptionCount,
        pVertexAttributeDescriptions);

    if (pBindState->pVertexInputInternalData != nullptr)
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            // Upload internal memory
            if (pBindState->hasDynamicVertexInput && (m_allGpuState.pGraphicsPipeline != nullptr))
            {
                VK_ASSERT(GetUberFetchShaderUserData(&pBindState->userDataLayout) != PipelineLayout::InvalidReg);

                PalCmdBuffer(deviceIdx)->CmdSetUserData(
                    Pal::PipelineBindPoint::Graphics,
                    GetUberFetchShaderUserData(&pBindState->userDataLayout),
                    2,
                    reinterpret_cast<uint32_t*>(&pBindState->pVertexInputInternalData->gpuAddress[deviceIdx]));
            }

            // Update vertex buffer stride
            uint32_t firstChanged = UINT_MAX;
            uint32_t lastChanged = 0;
            uint32_t vertexBufferCount = 0;
            Pal::BufferViewInfo* pVbBindings = PerGpuState(deviceIdx)->vbBindings;
            for (uint32_t bindex = 0; bindex < vertexBindingDescriptionCount; ++bindex)
            {
                uint32_t byteStride = pVertexBindingDescriptions[bindex].stride;
                uint32_t binding = pVertexBindingDescriptions[bindex].binding;

                vertexBufferCount = Util::Max(binding + 1, vertexBufferCount);

                Pal::BufferViewInfo* pBinding = &pVbBindings[binding];

                if (pBinding->stride != byteStride)
                {
                    pBinding->stride = byteStride;

                    if (pBinding->gpuAddr != 0)
                    {
                        firstChanged = Util::Min(firstChanged, binding);
                        lastChanged = Util::Max(lastChanged, binding);
                    }

                    if (padVertexBuffers && (pBinding->stride != 0))
                    {
                        pBinding->range = Util::RoundUpToMultiple(pBinding->range, pBinding->stride);
                    }
                }
            }

            if (firstChanged <= lastChanged)
            {
                PalCmdBuffer(deviceIdx)->CmdSetVertexBuffers(
                    firstChanged, (lastChanged - firstChanged) + 1,
                    &PerGpuState(deviceIdx)->vbBindings[firstChanged]);
            }

            if (vertexBufferCount != pBindState->dynamicBindInfo.gfx.dynamicState.vertexBufferCount)
            {
                pBindState->dynamicBindInfo.gfx.dynamicState.vertexBufferCount = vertexBufferCount;
                m_allGpuState.dirtyGraphics.pipeline = 1;
            }

        }
        while (deviceGroup.IterateNext());
    }
}

#if VK_ENABLE_DEBUG_BARRIERS
// =====================================================================================================================
// This function inserts a command before or after a particular Vulkan command if the given runtime settings are asking
// for it.
void CmdBuffer::DbgCmdBarrier(bool preCmd)
{
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

    static_assert(((static_cast<uint32_t>(Pal::HwPipePoint::HwPipeTop)              == HwPipeTop)              &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePostPrefetch)     == HwPipePostPrefetch)     &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePreRasterization) == HwPipePreRasterization) &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePostPs)           == HwPipePostPs)           &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePreColorTarget)   == HwPipePreColorTarget)   &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePostCs)           == HwPipePostCs)           &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipePostBlt)          == HwPipePostBlt)          &&
                   (static_cast<uint32_t>(Pal::HwPipePoint::HwPipeBottom)           == HwPipeBottom)),
        "The PAL::HwPipePoint enum has changed. Vulkan settings might need to be updated.");

    static_assert((
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherCpu)                == CoherCpu)                &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherShaderRead)         == CoherShaderRead)         &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherShaderWrite)        == CoherShaderWrite)        &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherCopySrc)            == CoherCopySrc)            &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherCopyDst)            == CoherCopyDst)            &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherColorTarget)        == CoherColorTarget)        &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherDepthStencilTarget) == CoherDepthStencilTarget) &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherResolveSrc)         == CoherResolveSrc)         &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherResolveDst)         == CoherResolveDst)         &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherClear)              == CoherClear)              &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherIndirectArgs)       == CoherIndirectArgs)       &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherIndexData)          == CoherIndexData)          &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherQueueAtomic)        == CoherQueueAtomic)        &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherTimestamp)          == CoherTimestamp)          &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherCeLoad)             == CoherCeLoad)             &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherCeDump)             == CoherCeDump)             &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherStreamOut)          == CoherStreamOut)          &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherMemory)             == CoherMemory)             &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherSampleRate)         == CoherSampleRate)         &&
        (static_cast<uint32_t>(Pal::CacheCoherencyUsageFlags::CoherPresent)            == CoherPresent)),
        "The PAL::CacheCoherencyUsageFlags enum has changed. Vulkan settings might need to be updated.");

    Pal::HwPipePoint waitPoint;
    Pal::HwPipePoint signalPoint;
    uint32_t         srcCacheMask;
    uint32_t         dstCacheMask;

    if (preCmd)
    {
        waitPoint    = static_cast<Pal::HwPipePoint>(settings.dbgBarrierPreWaitPipePoint);
        signalPoint  = static_cast<Pal::HwPipePoint>(settings.dbgBarrierPreSignalPipePoint);
        srcCacheMask = settings.dbgBarrierPreCacheSrcMask;
        dstCacheMask = settings.dbgBarrierPreCacheDstMask;
    }
    else
    {
        waitPoint    = static_cast<Pal::HwPipePoint>(settings.dbgBarrierPostWaitPipePoint);
        signalPoint  = static_cast<Pal::HwPipePoint>(settings.dbgBarrierPostSignalPipePoint);
        srcCacheMask = settings.dbgBarrierPostCacheSrcMask;
        dstCacheMask = settings.dbgBarrierPostCacheDstMask;
    }

    Pal::BarrierInfo barrier = {};

    barrier.reason    = RgpBarrierUnknownReason; // This code is debug-only code.
    barrier.waitPoint = waitPoint;

    if (waitPoint != Pal::HwPipeTop || signalPoint != Pal::HwPipeTop)
    {
        barrier.pipePointWaitCount = 1;
        barrier.pPipePoints        = &signalPoint;
    }

    Pal::BarrierTransition transition = {};

    if (srcCacheMask != 0 || dstCacheMask != 0)
    {
        transition.srcCacheMask = srcCacheMask;
        transition.dstCacheMask = dstCacheMask;

        barrier.transitionCount = 1;
        barrier.pTransitions    = &transition;
    }

    PalCmdBarrier(barrier, m_curDeviceMask);
}
#endif

// =====================================================================================================================
void CmdBuffer::WriteBufferMarker(
    PipelineStageFlags      pipelineStage,
    VkBuffer                dstBuffer,
    VkDeviceSize            dstOffset,
    uint32_t                marker)
{
    const Buffer* pDestBuffer        = Buffer::ObjectFromHandle(dstBuffer);
    const Pal::HwPipePoint pipePoint = VkToPalSrcPipePointForMarkers(pipelineStage, m_palEngineType);

    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdWriteImmediate(
            pipePoint,
            marker,
            Pal::ImmediateDataWidth::ImmediateData32Bit,
            pDestBuffer->GpuVirtAddr(deviceIdx) + dstOffset);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::BindTransformFeedbackBuffers(
    uint32_t            firstBinding,
    uint32_t            bindingCount,
    const VkBuffer*     pBuffers,
    const VkDeviceSize* pOffsets,
    const VkDeviceSize* pSizes)
{
    VK_ASSERT(firstBinding + bindingCount <= Pal::MaxStreamOutTargets);
    if (m_pTransformFeedbackState == nullptr)
    {
        void* pMemory = m_pDevice->VkInstance()->AllocMem(sizeof(TransformFeedbackState),
                                                          VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMemory != nullptr)
        {
            m_pTransformFeedbackState = reinterpret_cast<TransformFeedbackState*>(pMemory);
            memset(m_pTransformFeedbackState, 0, sizeof(TransformFeedbackState));
        }
        else
        {
            VK_NEVER_CALLED();
        }
    }

    if (m_pTransformFeedbackState != nullptr)
    {
        VK_ASSERT(m_pTransformFeedbackState->enabled == false);

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            for (uint32_t i = 0; i < bindingCount; i++)
            {
                uint32_t slot = i + firstBinding;
                if (pBuffers[i] != VK_NULL_HANDLE)
                {
                    Buffer* pFeedbackBuffer = Buffer::ObjectFromHandle(pBuffers[i]);

                    VkDeviceSize curSize = 0;
                    if ((pSizes == nullptr) || (pSizes[i] == VK_WHOLE_SIZE))
                    {
                        curSize = pFeedbackBuffer->GetSize() - pOffsets[i];
                    }
                    else
                    {
                        curSize = pSizes[i];
                    }

                    m_pTransformFeedbackState->params.target[slot].gpuVirtAddr =
                        pFeedbackBuffer->GpuVirtAddr(deviceIdx) + pOffsets[i];

                    m_pTransformFeedbackState->params.target[slot].size = curSize;

                    m_pTransformFeedbackState->bindMask |= 1 << slot;
                }
                else
                {
                    m_pTransformFeedbackState->params.target[slot].gpuVirtAddr = 0;
                    m_pTransformFeedbackState->params.target[slot].size        = 0;
                    m_pTransformFeedbackState->bindMask                        &= ~(1 << slot);
                }
            }
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::BeginTransformFeedback(
    uint32_t            firstCounterBuffer,
    uint32_t            counterBufferCount,
    const VkBuffer*     pCounterBuffers,
    const VkDeviceSize* pCounterBufferOffsets)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    if (m_pTransformFeedbackState != nullptr)
    {
        do
        {
            uint64_t counterBufferAddr[Pal::MaxStreamOutTargets] = {};

            const uint32_t deviceIdx = deviceGroup.Index();
            if (pCounterBuffers != nullptr)
            {
                CalcCounterBufferAddrs(firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets,
                                       counterBufferAddr, deviceIdx);
            }

            if (m_pTransformFeedbackState->bindMask != 0)
            {
                PalCmdBuffer(deviceIdx)->CmdBindStreamOutTargets(m_pTransformFeedbackState->params);
                PalCmdBuffer(deviceIdx)->CmdLoadBufferFilledSizes(counterBufferAddr);

                // If counter buffer is null, then stransform feedback will start capturing vertex data to byte offset zero.
                for (uint32_t i = 0; i < Pal::MaxStreamOutTargets; i++)
                {
                    if ((m_pTransformFeedbackState->bindMask & (1 << i)) && (counterBufferAddr[i] == 0))
                    {
                        PalCmdBuffer(deviceIdx)->CmdSetBufferFilledSize(i, 0);
                    }
                }

                m_pTransformFeedbackState->enabled = true;
            }
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::EndTransformFeedback(
    uint32_t            firstCounterBuffer,
    uint32_t            counterBufferCount,
    const VkBuffer*     pCounterBuffers,
    const VkDeviceSize* pCounterBufferOffsets)
{
    if ((m_pTransformFeedbackState != nullptr) && (m_pTransformFeedbackState->enabled))
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            uint64_t counterBufferAddr[Pal::MaxStreamOutTargets] = {};

            const uint32_t deviceIdx = deviceGroup.Index();
            if (pCounterBuffers != nullptr)
            {
                CalcCounterBufferAddrs(firstCounterBuffer, counterBufferCount, pCounterBuffers, pCounterBufferOffsets,
                                       counterBufferAddr, deviceIdx);
            }

            if (m_pTransformFeedbackState->bindMask != 0)
            {
                PalCmdBuffer(deviceIdx)->CmdSaveBufferFilledSizes(counterBufferAddr);

                // Disable transform feedback by set bound buffer's size and stride to 0.
                Pal::BindStreamOutTargetParams  params = {};
                PalCmdBuffer(deviceIdx)->CmdBindStreamOutTargets(params);
                m_pTransformFeedbackState->enabled = false;
            }
        }
        while (deviceGroup.IterateNext());
    }
}

// =====================================================================================================================
void CmdBuffer::CalcCounterBufferAddrs(
    uint32_t            firstCounterBuffer,
    uint32_t            counterBufferCount,
    const VkBuffer*     pCounterBuffers,
    const VkDeviceSize* pCounterBufferOffsets,
    uint64_t*           counterBufferAddr,
    uint32_t            deviceIdx)
{
    for (uint32_t i = firstCounterBuffer; i < (firstCounterBuffer + counterBufferCount); i++)
    {
        if ((pCounterBuffers[i] != VK_NULL_HANDLE) &&
            (m_pTransformFeedbackState->bindMask & (1 << i)))
        {
            Buffer* pCounterBuffer = Buffer::ObjectFromHandle(pCounterBuffers[i]);
            if (pCounterBufferOffsets != nullptr)
            {
                counterBufferAddr[i] = pCounterBuffer->GpuVirtAddr(deviceIdx) + pCounterBufferOffsets[i];
            }
            else
            {
                counterBufferAddr[i] = pCounterBuffer->GpuVirtAddr(deviceIdx);
            }
        }
    }
}

// =====================================================================================================================
void CmdBuffer::DrawIndirectByteCount(
    uint32_t        instanceCount,
    uint32_t        firstInstance,
    VkBuffer        counterBuffer,
    VkDeviceSize    counterBufferOffset,
    uint32_t        counterOffset,
    uint32_t        vertexStride)
{
    Buffer* pCounterBuffer = Buffer::ObjectFromHandle(counterBuffer);

    ValidateGraphicsStates();

#if VKI_RAY_TRACING
    BindRayQueryConstants(m_allGpuState.pGraphicsPipeline, Pal::PipelineBindPoint::Graphics, 0, 0, 0, nullptr, 0);
#endif

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx   = deviceGroup.Index();
        uint64_t counterBufferAddr = pCounterBuffer->GpuVirtAddr(deviceIdx) + counterBufferOffset;

        {
            PalCmdBuffer(deviceIdx)->CmdDrawOpaque(
                counterBufferAddr,
                counterOffset,
                vertexStride,
                firstInstance,
                instanceCount);
        }
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::SetLineStippleEXT(
    uint32_t lineStippleFactor,
    uint16_t lineStipplePattern)
{
    // The line stipple factor is adjusted by one (carried over from OpenGL)
    m_allGpuState.lineStipple.lineStippleScale = (lineStippleFactor - 1);

    // The bit field to describe the stipple pattern
    m_allGpuState.lineStipple.lineStippleValue = lineStipplePattern;

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetLineStippleState(m_allGpuState.lineStipple);
    }
    while (deviceGroup.IterateNext());

    m_allGpuState.staticTokens.lineStippleState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::CmdSetPerDrawVrsRate(
    const VkExtent2D*                        pFragmentSize,
    const VkFragmentShadingRateCombinerOpKHR combinerOps[2])
{
    m_allGpuState.vrsRate.shadingRate = VkToPalShadingSize(
        VkClampShadingRate(*pFragmentSize, m_pDevice->GetMaxVrsShadingRate()));

    m_allGpuState.vrsRate.combinerState[static_cast<uint32_t>(Pal::VrsCombinerStage::ProvokingVertex)] =
        VkToPalShadingRateCombinerOp(combinerOps[0]);

    m_allGpuState.vrsRate.combinerState[static_cast<uint32_t>(Pal::VrsCombinerStage::Primitive)] =
        VkToPalShadingRateCombinerOp(combinerOps[0]);

    m_allGpuState.vrsRate.combinerState[static_cast<uint32_t>(Pal::VrsCombinerStage::Image)] =
        VkToPalShadingRateCombinerOp(combinerOps[1]);

    m_allGpuState.vrsRate.combinerState[static_cast<uint32>(Pal::VrsCombinerStage::PsIterSamples)] =
        Pal::VrsCombiner::Passthrough;

    // Don't call CmdSetPerDrawVrsRate here since we have to observe the
    // currently bound pipeline to see if we should clamp the rate.
    // Calling Pal->CmdSetPerDrawVrsRate will happen in ValidateGraphicsStates
    m_allGpuState.dirtyGraphics.vrs                        = 1;
    m_allGpuState.staticTokens.fragmentShadingRate = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::CmdBeginConditionalRendering(
    const VkConditionalRenderingBeginInfoEXT* pConditionalRenderingBegin)
{
    // Make sure we have a properly aligned buffer offset.
    VK_ASSERT(Util::IsPow2Aligned(pConditionalRenderingBegin->offset, 4));

    // Conditional rendering discards the commands if the 32-bit value is zero.
    // Our hardware works in the opposite way, so we have to reverse the polarity flag.
    // PM4CMDSETPREDICATION:predicationBoolean:
    // 0 = draw_if_not_visible_or_overflow
    // 1 = draw_if_visible_or_no_overflow
    const bool predPolarity = (pConditionalRenderingBegin->flags & VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT) == 0;

    const Buffer* pBuffer = Buffer::ObjectFromHandle(pConditionalRenderingBegin->buffer);

    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetPredication(
            nullptr,
            0,
            pBuffer->PalMemory(deviceGroup.Index()),
            pBuffer->MemOffset() + pConditionalRenderingBegin->offset,
            Pal::PredicateType::Boolean32,
            predPolarity,
            false,
            false);
    }
    while (deviceGroup.IterateNext());

    m_flags.hasConditionalRendering = true;
}

// =====================================================================================================================
void CmdBuffer::CmdEndConditionalRendering()
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetPredication(
            nullptr,
            0,
            nullptr,
            0,
            Pal::PredicateType::Boolean32,
            false,
            false,
            false);
    }
    while (deviceGroup.IterateNext());

    m_flags.hasConditionalRendering = false;
}

// =====================================================================================================================
void CmdBuffer::CmdDebugMarkerBegin(
    const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
    InsertDebugMarker(pMarkerInfo->pMarkerName, true);
}

// =====================================================================================================================
void CmdBuffer::CmdDebugMarkerEnd()
{
    InsertDebugMarker(nullptr, false);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginDebugUtilsLabel(
    const VkDebugUtilsLabelEXT* pLabelInfo)
{
    InsertDebugMarker(pLabelInfo->pLabelName, true);
}

// =====================================================================================================================
void CmdBuffer::CmdEndDebugUtilsLabel()
{
    InsertDebugMarker(nullptr, false);
}

// =====================================================================================================================
void CmdBuffer::BindAlternatingThreadGroupConstant()
{
    uint32_t              data            = m_reverseThreadGroupState ? 1 : 0;
    const UserDataLayout* pUserDataLayout = m_allGpuState.pComputePipeline->GetUserDataLayout();
    uint32_t              userDataRegBase = (pUserDataLayout->scheme == PipelineLayoutScheme::Compact) ?
                                                pUserDataLayout->compact.threadGroupReversalRegBase :
                                                pUserDataLayout->indirect.threadGroupReversalRegBase;

    if (userDataRegBase != PipelineLayout::InvalidReg)
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t   deviceIdx     = deviceGroup.Index();
            Pal::ICmdBuffer* pPalCmdBuffer = PalCmdBuffer(deviceIdx);
            Pal::gpusize     constGpuAddr  = 0;

            void* pConstData = pPalCmdBuffer->CmdAllocateEmbeddedData(1, 1, &constGpuAddr);
            memcpy(pConstData, &data, sizeof(data));

            pPalCmdBuffer->CmdSetUserData(
                Pal::PipelineBindPoint::Compute, userDataRegBase, 2, reinterpret_cast<uint32_t*>(&constGpuAddr));
        }
        while (deviceGroup.IterateNext());
    }

    // Flip the reversal state
    m_reverseThreadGroupState = (m_reverseThreadGroupState == false);
}

#if VKI_RAY_TRACING
// =====================================================================================================================
void CmdBuffer::BuildAccelerationStructures(
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos,
    const VkDeviceAddress*                                  pIndirectDeviceAddresses,
    const uint32*                                           pIndirectStrides,
    const uint32* const*                                    ppMaxPrimitiveCounts)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        BuildAccelerationStructuresPerDevice(
            deviceIdx,
            infoCount,
            pInfos,
            ppBuildRangeInfos,
            pIndirectDeviceAddresses,
            pIndirectStrides,
            ppMaxPrimitiveCounts);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::BuildAccelerationStructuresPerDevice(
    const uint32_t                                          deviceIndex,
    uint32_t                                                infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR*      pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const*  ppBuildRangeInfos,
    const VkDeviceAddress*                                  pIndirectDeviceAddresses,
    const uint32*                                           pIndirectStrides,
    const uint32* const*                                    ppMaxPrimitiveCounts)
{
    for (uint32_t infoIdx = 0; infoIdx < infoCount; ++infoIdx)
    {
        const VkAccelerationStructureBuildGeometryInfoKHR* pInfo         = &pInfos[infoIdx];
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = (ppBuildRangeInfos != nullptr) ?
                                                                            ppBuildRangeInfos[infoIdx] :
                                                                            nullptr;

        AccelerationStructure* pDst = AccelerationStructure::ObjectFromHandle(pInfo->dstAccelerationStructure);
        const AccelerationStructure* pSrc = AccelerationStructure::ObjectFromHandle(pInfo->srcAccelerationStructure);

        // pDst must be a valid handle
        VK_ASSERT(pDst != nullptr);

        GpuRt::AccelStructBuildInfo info = {};

        info.dstAccelStructGpuAddr = (pDst != nullptr) ? pDst->GetDeviceAddress(deviceIndex) : 0;
        info.srcAccelStructGpuAddr = ((pInfo->mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR) &&
                                      (pSrc != nullptr)) ? pSrc->GetDeviceAddress(deviceIndex) : 0;

        GeometryConvertHelper helper = {};

        AccelerationStructure::ConvertBuildInputsKHR(
            false,
            VkDevice(),
            deviceIndex,
            *pInfo,
            pBuildRangeInfos,
            &helper,
            &info.inputs);

        info.scratchAddr.gpu = pInfo->scratchData.deviceAddress;

        // Set Indirect Values
        if (pIndirectDeviceAddresses != nullptr)
        {
            VK_ASSERT(pIndirectDeviceAddresses[infoIdx] > 0);

            info.indirect.indirectGpuAddr  = pIndirectDeviceAddresses[infoIdx];
            info.indirect.indirectStride   = pIndirectStrides[infoIdx];
            helper.pMaxPrimitiveCounts     = ppMaxPrimitiveCounts[infoIdx];
        }

        DbgBarrierPreCmd((pInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) ?
                            DbgBuildAccelerationStructureTLAS : DbgBuildAccelerationStructureBLAS);

        m_pDevice->RayTrace()->GpuRt(deviceIndex)->BuildAccelStruct(
                PalCmdBuffer(deviceIndex),
                info);

        DbgBarrierPostCmd((pInfo->type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) ?
                            DbgBuildAccelerationStructureTLAS : DbgBuildAccelerationStructureBLAS);
    }
}

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
void CmdBuffer::WriteAccelerationStructuresProperties(
    uint32_t                                    accelerationStructureCount,
    const VkAccelerationStructureKHR*           pAccelerationStructures,
    VkQueryType                                 queryType,
    VkQueryPool                                 queryPool,
    uint32_t                                    firstQuery)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        WriteAccelerationStructuresPropertiesPerDevice(
            deviceIdx,
            accelerationStructureCount,
            pAccelerationStructures,
            queryType,
            queryPool,
            firstQuery);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::WriteAccelerationStructuresPropertiesPerDevice(
    const uint32_t                    deviceIndex,
    uint32_t                          accelerationStructureCount,
    const VkAccelerationStructureKHR* pAccelerationStructures,
    VkQueryType                       queryType,
    VkQueryPool                       queryPool,
    uint32_t                          firstQuery)
{
    VK_ASSERT(IsAccelerationStructureQueryType(queryType));

    GpuRt::AccelStructPostBuildInfo postBuildInfo = {};

    postBuildInfo.srcAccelStructCount = 1;

    switch (static_cast<uint32_t>(queryType))
    {
    case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SIZE_KHR:
        postBuildInfo.desc.infoType = GpuRt::AccelStructPostBuildInfoType::CurrentSize;
        break;
    case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_BOTTOM_LEVEL_POINTERS_KHR:
        postBuildInfo.desc.infoType = GpuRt::AccelStructPostBuildInfoType::Serialization;
        break;
    case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR:
        postBuildInfo.desc.infoType = GpuRt::AccelStructPostBuildInfoType::CompactedSize;
        break;
    case VK_QUERY_TYPE_ACCELERATION_STRUCTURE_SERIALIZATION_SIZE_KHR:
        postBuildInfo.desc.infoType = GpuRt::AccelStructPostBuildInfoType::Serialization;
        break;
    default:
        VK_NEVER_CALLED();
        break;
    }

    const AccelerationStructureQueryPool* pQueryPool =
        QueryPool::ObjectFromHandle(queryPool)->AsAccelerationStructureQueryPool();

    const uint32_t emitSize         = pQueryPool->GetSlotSize();
    const Pal::gpusize basePoolAddr = pQueryPool->GpuVirtAddr(deviceIndex);
    GpuRt::IDevice* const pGpuRt    = m_pDevice->RayTrace()->GpuRt(deviceIndex);

    for (uint32_t i = 0; i < accelerationStructureCount; i++)
    {
        const AccelerationStructure* pAccelStructure =
            AccelerationStructure::ObjectFromHandle(pAccelerationStructures[i]);

        Pal::gpusize gpuAddr = pAccelStructure->GetDeviceAddress(deviceIndex);

        postBuildInfo.desc.postBuildBufferAddr.gpu = basePoolAddr + ((firstQuery + i) * emitSize);
        postBuildInfo.pSrcAccelStructGpuAddrs      = &gpuAddr;

        pGpuRt->EmitAccelStructPostBuildInfo(PalCmdBuffer(deviceIndex), postBuildInfo);
    }
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

// =====================================================================================================================
void CmdBuffer::TraceRays(
    const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
    uint32_t                        width,
    uint32_t                        height,
    uint32_t                        depth)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        TraceRaysPerDevice(
            deviceIdx,
            raygenShaderBindingTable,
            missShaderBindingTable,
            hitShaderBindingTable,
            callableShaderBindingTable,
            width,
            height,
            depth);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
void CmdBuffer::GetRayTracingDispatchArgs(
    uint32_t                               deviceIdx,
    const RuntimeSettings&                 settings,
    CmdPool*                               pCmdPool,
    const RayTracingPipeline*              pPipeline,
    Pal::gpusize                           constGpuAddr,
    uint32_t                               width,
    uint32_t                               height,
    uint32_t                               depth,
    const VkStridedDeviceAddressRegionKHR& raygenSbt,
    const VkStridedDeviceAddressRegionKHR& missSbt,
    const VkStridedDeviceAddressRegionKHR& hitSbt,
    const VkStridedDeviceAddressRegionKHR& callableSbt,
    GpuRt::DispatchRaysConstants*          pConstants)
{
    pConstants->constData.rayGenerationTableAddressLo = Util::LowPart(raygenSbt.deviceAddress);
    pConstants->constData.rayGenerationTableAddressHi = Util::HighPart(raygenSbt.deviceAddress);

    pConstants->constData.rayDispatchWidth            = width;
    pConstants->constData.rayDispatchHeight           = height;
    pConstants->constData.rayDispatchDepth            = depth;
    pConstants->constData.missTableBaseAddressLo     = Util::LowPart(missSbt.deviceAddress);
    pConstants->constData.missTableBaseAddressHi     = Util::HighPart(missSbt.deviceAddress);
    pConstants->constData.missTableStrideInBytes     = static_cast<uint32_t>(missSbt.stride);

    pConstants->constData.hitGroupTableBaseAddressLo = Util::LowPart(hitSbt.deviceAddress);
    pConstants->constData.hitGroupTableBaseAddressHi = Util::HighPart(hitSbt.deviceAddress);
    pConstants->constData.hitGroupTableStrideInBytes = static_cast<uint32_t>(hitSbt.stride);

    pConstants->constData.callableTableBaseAddressLo = Util::LowPart(callableSbt.deviceAddress);
    pConstants->constData.callableTableBaseAddressHi = Util::HighPart(callableSbt.deviceAddress);
    pConstants->constData.callableTableStrideInBytes = static_cast<uint32_t>(callableSbt.stride);

    pConstants->constData.traceRayGpuVaLo            = Util::LowPart(pPipeline->GetTraceRayGpuVa(deviceIdx));
    pConstants->constData.traceRayGpuVaHi            = Util::HighPart(pPipeline->GetTraceRayGpuVa(deviceIdx));
    pConstants->constData.profileMaxIterations       = m_pDevice->RayTrace()->GetProfileMaxIterations();
    pConstants->constData.profileRayFlags            = m_pDevice->RayTrace()->GetProfileRayFlags();

    pConstants->descriptorTable.dispatchRaysConstGpuVa = constGpuAddr +
                                                         offsetof(GpuRt::DispatchRaysConstants, constData);

    memcpy(pConstants->descriptorTable.accelStructTrackerSrd,
           m_pDevice->RayTrace()->GetAccelStructTrackerSrd(deviceIdx),
           sizeof(pConstants->descriptorTable.accelStructTrackerSrd));

    static_assert(uint32_t(GpuRt::TraceRayCounterDisable) == uint32_t(TraceRayCounterDisable),
                  "Wrong enum value, TraceRayCounterDisable != GpuRt::TraceRayCounterDisable");
    static_assert(uint32_t(GpuRt::TraceRayCounterRayHistoryLight) == uint32_t(TraceRayCounterRayHistoryLight),
                  "Wrong enum value, TraceRayCounterRayHistoryLight != GpuRt::TraceRayCounterRayHistoryLight");
    static_assert(uint32_t(GpuRt::TraceRayCounterRayHistoryFull) == uint32_t(TraceRayCounterRayHistoryFull),
                  "Wrong enum value, TraceRayCounterRayHistoryFull != GpuRt::TraceRayCounterRayHistoryFull");
    static_assert(uint32_t(GpuRt::TraceRayCounterTraversal) == uint32_t(TraceRayCounterTraversal),
                  "Wrong enum value, TraceRayCounterTraversal != GpuRt::TraceRayCounterTraversal");
    static_assert(uint32_t(GpuRt::TraceRayCounterCustom) == uint32_t(TraceRayCounterCustom),
                  "Wrong enum value, TraceRayCounterCustom != GpuRt::TraceRayCounterCustom");
    static_assert(uint32_t(GpuRt::TraceRayCounterDispatch) == uint32_t(TraceRayCounterDispatch),
                  "Wrong enum value, TraceRayCounterDispatch != GpuRt::TraceRayCounterDispatch");

    if (width > 0)
    {
        // Populate internalUavBufferSrd only for direct dispatches (where width, height, and depth are known)
        m_pDevice->RayTrace()->TraceDispatch(deviceIdx,
                                             PalCmdBuffer(deviceIdx),
                                             GpuRt::RtPipelineType::RayTracing,
                                             width,
                                             height,
                                             depth,
                                             pPipeline->GetShaderGroupCount() + 1,
                                             pPipeline->GetApiHash(),
                                             &raygenSbt,
                                             &missSbt,
                                             &hitSbt,
                                             pConstants);
    }

}

// =====================================================================================================================
void CmdBuffer::TraceRaysPerDevice(
    const uint32_t                         deviceIdx,
    const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
    uint32_t                               width,
    uint32_t                               height,
    uint32_t                               depth)
{
    DbgBarrierPreCmd(DbgTraceRays);

    const RuntimeSettings& settings     = m_pDevice->GetRuntimeSettings();
    const RayTracingPipeline* pPipeline = m_allGpuState.pRayTracingPipeline;

    UpdateLargestPipelineStackSize(deviceIdx, pPipeline->GetDefaultPipelineStackSize(deviceIdx));

    Pal::gpusize constGpuAddr;

    void* pConstData = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(GpuRt::DispatchRaysConstantsDw,
                                                                        1,
                                                                        &constGpuAddr);

    GpuRt::DispatchRaysConstants constants = {};

    GetRayTracingDispatchArgs(deviceIdx,
                              m_pDevice->GetRuntimeSettings(),
                              m_pCmdPool,
                              pPipeline,
                              constGpuAddr,
                              width,
                              height,
                              depth,
                              raygenShaderBindingTable,
                              missShaderBindingTable,
                              hitShaderBindingTable,
                              callableShaderBindingTable,
                              &constants);

    memcpy(pConstData, &constants, sizeof(constants));

    if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindRayTracing) == false)
    {
        RebindPipeline<PipelineBindRayTracing, false>();
    }

    uint32_t dispatchRaysUserData = pPipeline->GetDispatchRaysUserDataOffset();
    uint32_t constGpuAddrLow      = Util::LowPart(constGpuAddr);

    PalCmdBuffer(deviceIdx)->CmdSetUserData(Pal::PipelineBindPoint::Compute, dispatchRaysUserData, 1, &constGpuAddrLow);

    uint32_t dispatchSizeX = 0;
    uint32_t dispatchSizeY = 0;
    uint32_t dispatchSizeZ = 0;

    pPipeline->GetDispatchSize(&dispatchSizeX, &dispatchSizeY, &dispatchSizeZ, width, height, depth);

    PalCmdBuffer(deviceIdx)->CmdDispatch({ dispatchSizeX, dispatchSizeY, dispatchSizeZ });

    DbgBarrierPostCmd(DbgTraceRays);
}

// =====================================================================================================================
void CmdBuffer::TraceRaysIndirect(
    GpuRt::ExecuteIndirectArgType          indirectArgType,
    const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
    VkDeviceAddress                        indirectDeviceAddress)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        TraceRaysIndirectPerDevice(
            deviceIdx,
            indirectArgType,
            raygenShaderBindingTable,
            missShaderBindingTable,
            hitShaderBindingTable,
            callableShaderBindingTable,
            indirectDeviceAddress);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Sets a barrier from indirect_arg state to copy_source for rayquery copy arguments.
void CmdBuffer::SyncIndirectCopy(
    Pal::ICmdBuffer* pCmdBuffer)
{
    if (m_pDevice->GetRuntimeSettings().useAcquireReleaseInterface)
    {
        Pal::AcquireReleaseInfo acqRelInfo = {};
        Pal::MemBarrier         memTransition = {};

        memTransition.srcAccessMask = Pal::CoherIndirectArgs;
        memTransition.dstAccessMask = Pal::CoherCopySrc | Pal::CoherIndirectArgs;
        memTransition.srcStageMask  = Pal::PipelineStageCs;
        memTransition.dstStageMask  = Pal::PipelineStageBlt;

        acqRelInfo.pMemoryBarriers      = &memTransition;
        acqRelInfo.memoryBarrierCount   = 1;
        acqRelInfo.reason               = RgpBarrierInternalRayTracingSync;

        pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
    }
    else
    {
        Pal::BarrierTransition transition   = {};
        transition.srcCacheMask             = Pal::CoherIndirectArgs;
        transition.dstCacheMask             = Pal::CoherCopySrc | Pal::CoherIndirectArgs;

        const Pal::HwPipePoint postBlt      = Pal::HwPipePreBlt;

        Pal::BarrierInfo barrierInfo    = {};
        barrierInfo.pipePointWaitCount  = 1;
        barrierInfo.pPipePoints         = &postBlt;
        barrierInfo.waitPoint           = Pal::HwPipeTop;
        barrierInfo.transitionCount     = 1;
        barrierInfo.pTransitions        = &transition;
        barrierInfo.reason              = RgpBarrierInternalRayTracingSync;

        pCmdBuffer->CmdBarrier(barrierInfo);
    }
}

// =====================================================================================================================
void CmdBuffer::TraceRaysIndirectPerDevice(
    const uint32_t                         deviceIdx,
    GpuRt::ExecuteIndirectArgType          indirectArgType,
    const VkStridedDeviceAddressRegionKHR& raygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& missShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& hitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR& callableShaderBindingTable,
    VkDeviceAddress                        indirectDeviceAddress)
{
    DbgBarrierPreCmd(DbgTraceRays);

    const RuntimeSettings& settings     = m_pDevice->GetRuntimeSettings();
    const RayTracingPipeline* pPipeline = m_allGpuState.pRayTracingPipeline;

    UpdateLargestPipelineStackSize(deviceIdx, pPipeline->GetDefaultPipelineStackSize(deviceIdx));

    // Fill the dispatch launch constants
    Pal::gpusize constGpuAddr;

    void* pConstData = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(GpuRt::DispatchRaysConstantsDw,
                                                                        1,
                                                                        &constGpuAddr);

    GpuRt::DispatchRaysConstants constants = {};

    GetRayTracingDispatchArgs(deviceIdx,
                              m_pDevice->GetRuntimeSettings(),
                              m_pCmdPool,
                              pPipeline,
                              constGpuAddr,
                              0, // Pre-pass will populate width x height x depth
                              0,
                              0,
                              raygenShaderBindingTable,
                              missShaderBindingTable,
                              hitShaderBindingTable,
                              callableShaderBindingTable,
                              &constants);

    memcpy(pConstData, &constants, sizeof(constants));

    // Pre-pass
    gpusize initConstantsVa = 0;

    const gpusize scratchBufferSize = sizeof(VkTraceRaysIndirectCommandKHR);

    InternalMemory* pScratchMemory  = nullptr;
    VkResult result                 = GetRayTracingIndirectMemory(scratchBufferSize, &pScratchMemory);

    VK_ASSERT(result == VK_SUCCESS);

    m_rayTracingIndirectList.PushBack(pScratchMemory);

    auto* pInitConstants = reinterpret_cast<GpuRt::InitExecuteIndirectConstants*>(
        PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(GpuRt::InitExecuteIndirectConstantsDw,
                                                         2,
                                                         &initConstantsVa));

    pInitConstants->maxIterations   = m_pDevice->RayTrace()->GetProfileMaxIterations();
    pInitConstants->profileRayFlags = m_pDevice->RayTrace()->GetProfileRayFlags();

    pInitConstants->maxDispatchCount   = 1;
    pInitConstants->pipelineCount      = 1;
#if GPURT_INTERFACE_VERSION >= MAKE_GPURT_VERSION(11,3)
    pInitConstants->indirectMode       =
        (indirectArgType == GpuRt::ExecuteIndirectArgType::DispatchDimensions) ? 0 : 1;
#endif

    if (settings.rtFlattenThreadGroupSize == 0)
    {
        pInitConstants->dispatchDimSwizzleMode = 0;
        pInitConstants->rtThreadGroupSizeX     = settings.rtThreadGroupSizeX;
        pInitConstants->rtThreadGroupSizeY     = settings.rtThreadGroupSizeY;
        pInitConstants->rtThreadGroupSizeZ     = settings.rtThreadGroupSizeZ;
    }
    else
    {
        pInitConstants->dispatchDimSwizzleMode = 1;
        pInitConstants->rtThreadGroupSizeX     = settings.rtFlattenThreadGroupSize;
        pInitConstants->rtThreadGroupSizeY     = 1;
        pInitConstants->rtThreadGroupSizeZ     = 1;
    }

    GpuRt::InitExecuteIndirectUserData initUserData = {};

    initUserData.constantsVa            = initConstantsVa;
    initUserData.inputBufferVa          = indirectDeviceAddress;
    initUserData.outputBufferVa         = pScratchMemory->GpuVirtAddr(deviceIdx);
    initUserData.outputConstantsVa      = constants.descriptorTable.dispatchRaysConstGpuVa;
    initUserData.outputCounterMetaVa    = 0uLL;

    m_pDevice->RayTrace()->TraceIndirectDispatch(deviceIdx,
                                                 GpuRt::RtPipelineType::RayTracing,
                                                 0,
                                                 0,
                                                 0,
                                                 pPipeline->GetShaderGroupCount() + 1,
                                                 pPipeline->GetApiHash(),
                                                 &raygenShaderBindingTable,
                                                 &missShaderBindingTable,
                                                 &hitShaderBindingTable,
                                                 &initUserData.outputCounterMetaVa,
                                                 pInitConstants);

    m_pDevice->RayTrace()->GpuRt(deviceIdx)->InitExecuteIndirect(PalCmdBuffer(deviceIdx), initUserData, 1, 1);

    // Wait for the argument buffer to be populated before continuing with TraceRaysIndirect
    const Pal::HwPipePoint postCs = Pal::HwPipePostCs;

    Pal::BarrierInfo barrier = {};

    barrier.pipePointWaitCount = 1;
    barrier.pPipePoints        = &postCs;
    barrier.waitPoint          = Pal::HwPipeTop;

    Pal::BarrierTransition transition = {};

    transition.srcCacheMask = Pal::CoherShaderWrite;
    transition.dstCacheMask = Pal::CoherShaderRead | Pal::CoherIndirectArgs;

    barrier.transitionCount = 1;
    barrier.pTransitions    = &transition;
    barrier.reason          = Pal::Developer::BarrierReasonUnknown;

    PalCmdBarrier(barrier, m_curDeviceMask);

    uint32_t dispatchRaysUserData = pPipeline->GetDispatchRaysUserDataOffset();
    uint32_t constGpuAddrLow      = uint32_t(constGpuAddr);

    // Switch to the raytracing pipeline if needed
    if (PalPipelineBindingOwnedBy(Pal::PipelineBindPoint::Compute, PipelineBindRayTracing) == false)
    {
        RebindPipeline<PipelineBindRayTracing, false>();
    }

    PalCmdBuffer(deviceIdx)->CmdSetUserData(Pal::PipelineBindPoint::Compute,
                                            dispatchRaysUserData,
                                            1,
                                            &constGpuAddrLow);

    PalCmdBuffer(deviceIdx)->CmdDispatchIndirect(*pScratchMemory->PalMemory(deviceIdx), pScratchMemory->Offset());

    DbgBarrierPostCmd(DbgTraceRays);
}

// =====================================================================================================================
// Alloacates GPU video memory according for TraceRaysIndirect
VkResult CmdBuffer::GetRayTracingIndirectMemory(
    gpusize          size,
    InternalMemory** ppInternalMemory)
{
    VkResult result = VK_SUCCESS;

    *ppInternalMemory = nullptr;

    // Allocate system memory for InternalMemory object
    InternalMemory* pInternalMemory = nullptr;

    void* pSystemMemory = m_pDevice->VkInstance()->AllocMem(sizeof(InternalMemory),
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pSystemMemory != nullptr)
    {
        pInternalMemory = VK_PLACEMENT_NEW(pSystemMemory) InternalMemory;
    }

    // Allocate GPU video memory
    if (pInternalMemory != nullptr)
    {
        InternalMemCreateInfo allocInfo = {};

        allocInfo.pal.size      = size;
        allocInfo.pal.alignment = 16;
        allocInfo.pal.priority  = Pal::GpuMemPriority::Normal;

        m_pDevice->MemMgr()->GetCommonPool(InternalPoolGpuAccess, &allocInfo);

        result = m_pDevice->MemMgr()->AllocGpuMem(
            allocInfo,
            pInternalMemory,
            m_pDevice->GetPalDeviceMask(),
            VK_OBJECT_TYPE_COMMAND_BUFFER,
            ApiCmdBuffer::IntValueFromHandle(ApiCmdBuffer::FromObject(this)));

        VK_ASSERT(result == VK_SUCCESS);

        if (result == VK_SUCCESS)
        {
            *ppInternalMemory = pInternalMemory;
        }
    }

    if (result != VK_SUCCESS)
    {
        // Clean up if fail
        if (pInternalMemory != nullptr)
        {
            Util::Destructor(pInternalMemory);
        }

        m_pDevice->VkInstance()->FreeMem(pSystemMemory);
    }

    return result;
}

// =====================================================================================================================
// Free GPU video memory according for TraceRaysIndirect
void CmdBuffer::FreeRayTracingIndirectMemory()
{
    // This data could be farily large and consumes framebuffer memory.
    //
    // This should always be done when vkResetCommandBuffer() is called to handle the case
    // where an app resets a command buffer but doesn't call vkBeginCommandBuffer right away.
    for (uint32_t i = 0; i < m_rayTracingIndirectList.NumElements(); ++i)
    {
        // Dump entry data
        InternalMemory* indirectMemory = m_rayTracingIndirectList.At(i);

        // Free memory
        m_pDevice->MemMgr()->FreeGpuMem(indirectMemory);

        Util::Destructor(indirectMemory);
        m_pDevice->VkInstance()->FreeMem(indirectMemory);
    }

    // Clear list
    m_rayTracingIndirectList.Clear();
}

// =====================================================================================================================
// Set the dynamic stack size for a ray tracing pipeline
void CmdBuffer::SetRayTracingPipelineStackSize(
    uint32_t pipelineStackSize)
{
    utils::IterateMask deviceGroup(m_curDeviceMask);

    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        UpdateLargestPipelineStackSize(deviceIdx, pipelineStackSize);
    }
    while (deviceGroup.IterateNext());
}

// =====================================================================================================================
// Setup internal constants and descriptors required for shaders using RayQuery
void CmdBuffer::BindRayQueryConstants(
    const Pipeline*        pPipeline,
    Pal::PipelineBindPoint bindPoint,
    uint32_t               width,
    uint32_t               height,
    uint32_t               depth,
    Buffer*                pIndirectBuffer,
    VkDeviceSize           indirectOffset)
{
    if ((pPipeline != nullptr) && pPipeline->HasRayTracing())
    {
        utils::IterateMask deviceGroup(m_curDeviceMask);

        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            const bool asTrackingEnabled = VkDevice()->RayTrace()->AccelStructTrackerEnabled(deviceIdx);
            const bool rtCountersEnabled = VkDevice()->RayTrace()->RayHistoryTraceActive(deviceIdx);

            if (asTrackingEnabled || rtCountersEnabled)
            {
                GpuRt::DispatchRaysConstants constants     = {};
                Pal::ICmdBuffer*             pPalCmdBuffer = PalCmdBuffer(deviceIdx);
                Pal::gpusize                 constGpuAddr  = 0;

                void* pConstData = pPalCmdBuffer->CmdAllocateEmbeddedData(GpuRt::DispatchRaysConstantsDw,
                                                                          1,
                                                                          &constGpuAddr);

                if (asTrackingEnabled)
                {
                    memcpy(constants.descriptorTable.accelStructTrackerSrd,
                        VkDevice()->RayTrace()->GetAccelStructTrackerSrd(deviceIdx),
                        sizeof(constants.descriptorTable.accelStructTrackerSrd));
                }

                if (rtCountersEnabled)
                {
                    constants.descriptorTable.dispatchRaysConstGpuVa = constGpuAddr +
                        offsetof(GpuRt::DispatchRaysConstants, constData);

                    // Ray history dumps for Graphics pipelines are not yet supported
                    if (bindPoint == Pal::PipelineBindPoint::Compute)
                    {
                        const uint32_t* pOrigThreadgroupDims =
                            static_cast<const ComputePipeline*>(pPipeline)->GetOrigThreadgroupDims();

                        constants.constData.profileMaxIterations = m_pDevice->RayTrace()->GetProfileMaxIterations();
                        constants.constData.profileRayFlags      = m_pDevice->RayTrace()->GetProfileRayFlags();

                        gpusize indirectBufferVa = (pIndirectBuffer != nullptr) ?
                            pIndirectBuffer->GpuVirtAddr(deviceIdx) + indirectOffset :
                            0;

                        if (indirectBufferVa == 0)
                        {
                            constants.constData.rayDispatchWidth  = width  * pOrigThreadgroupDims[0];
                            constants.constData.rayDispatchHeight = height * pOrigThreadgroupDims[1];
                            constants.constData.rayDispatchDepth  = depth  * pOrigThreadgroupDims[2];

                            m_pDevice->RayTrace()->TraceDispatch(deviceIdx,
                                                                 PalCmdBuffer(deviceIdx),
                                                                 GpuRt::RtPipelineType::Compute,
                                                                 width  * pOrigThreadgroupDims[0],
                                                                 height * pOrigThreadgroupDims[1],
                                                                 depth  * pOrigThreadgroupDims[2],
                                                                 1,
                                                                 pPipeline->GetApiHash(),
                                                                 nullptr,
                                                                 nullptr,
                                                                 nullptr,
                                                                 &constants);
                        }
                        else
                        {
                            uint64 counterMetadataGpuVa = 0uLL;

                            m_pDevice->RayTrace()->TraceIndirectDispatch(deviceIdx,
                                                                         GpuRt::RtPipelineType::Compute,
                                                                         pOrigThreadgroupDims[0],
                                                                         pOrigThreadgroupDims[1],
                                                                         pOrigThreadgroupDims[2],
                                                                         1,
                                                                         pPipeline->GetApiHash(),
                                                                         nullptr,
                                                                         nullptr,
                                                                         nullptr,
                                                                         &counterMetadataGpuVa,
                                                                         &constants);

                            Pal::MemoryCopyRegion region = {};
                            region.srcOffset = 0;
                            region.copySize = sizeof(GpuRt::IndirectCounterMetadata) - sizeof(uint64);

                            SyncIndirectCopy(PalCmdBuffer(deviceIdx));
                            PalCmdBuffer(deviceIdx)->CmdCopyMemoryByGpuVa(
                                indirectBufferVa,
                                (counterMetadataGpuVa + offsetof(GpuRt::IndirectCounterMetadata, dispatchRayDimensionX)),
                                1,
                                &region);
                        }
                    }
                }

                memcpy(pConstData, &constants, sizeof(constants));

                uint32_t dispatchRaysUserData = pPipeline->GetDispatchRaysUserDataOffset();
                uint32_t constGpuAddrLow      = Util::LowPart(constGpuAddr);

                pPalCmdBuffer->CmdSetUserData(bindPoint, dispatchRaysUserData, 1, &constGpuAddrLow);
            }
        }
        while (deviceGroup.IterateNext());
    }
}

#endif

// =====================================================================================================================
void CmdBuffer::InsertDebugMarker(
    const char* pLabelName,
    bool        isBegin)
{
#if ICD_GPUOPEN_DEVMODE_BUILD
    constexpr uint8 MarkerSourceApplication = 0;

    const DevModeMgr* pDevModeMgr = m_pDevice->VkInstance()->GetDevModeMgr();

    // Insert Crash Analysis markers if requested
    if ((pDevModeMgr != nullptr) && (pDevModeMgr->IsCrashAnalysisEnabled()))
    {
        PalCmdBuffer(DefaultDeviceIndex)->CmdInsertExecutionMarker(isBegin,
                                                                   MarkerSourceApplication,
                                                                   pLabelName,
                                                                   (pLabelName != nullptr) ?
                                                                   Util::StringLength(pLabelName) :
                                                                   0);
    }
#endif
}

// =====================================================================================================================
void CmdBuffer::BindDescriptorBuffers(
    uint32_t                                bufferCount,
    const VkDescriptorBufferBindingInfoEXT* pBindingInfos)
{
    // Please check if EXT_DESCRIPTOR_BUFFER is enabled.
    VK_ASSERT(m_allGpuState.pDescBufBinding != nullptr);

    VK_ASSERT(bufferCount <= MaxDescriptorSets);

    for (uint32_t ndx = 0; ndx < bufferCount; ++ndx)
    {
        VK_ASSERT(pBindingInfos[ndx].sType == VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT);
        m_allGpuState.pDescBufBinding->baseAddr[ndx] = pBindingInfos[ndx].address;
    }
}

// =====================================================================================================================
void CmdBuffer::SetDescriptorBufferOffsets(
    VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout    layout,
    uint32_t            firstSet,
    uint32_t            setCount,
    const uint32_t*     pBufferIndices,
    const VkDeviceSize* pOffsets)
{
    // Please check if EXT_DESCRIPTOR_BUFFER is enabled.
    VK_ASSERT(m_allGpuState.pDescBufBinding != nullptr);

    DescriptorBuffers   descBuffers[MaxDescriptorSets] = {};

    for (uint32_t ndx = 0u; ndx < setCount; ++ndx)
    {
        const uint32_t descNdx           = ndx + firstSet;
        descBuffers[descNdx].offset      = pOffsets[ndx];
        descBuffers[descNdx].baseAddrNdx = pBufferIndices[ndx];

        // First baseAddr should be bound by BindDescriptorBuffers.
        VK_ASSERT(m_allGpuState.pDescBufBinding->baseAddr[pBufferIndices[ndx]] != 0);
    }

    BindDescriptorSetsBuffers(pipelineBindPoint,
                              layout,
                              firstSet,
                              setCount,
                              descBuffers);
}

// =====================================================================================================================
void CmdBuffer::BindDescriptorBufferEmbeddedSamplers(
    VkPipelineBindPoint     pipelineBindPoint,
    VkPipelineLayout        layout,
    uint32_t                set)
{
    const PipelineLayout*                    pLayout       = PipelineLayout::ObjectFromHandle(layout);
    const PipelineLayout::SetUserDataLayout& setLayoutInfo = pLayout->GetSetUserData(set);

    VK_ASSERT(set <= pLayout->GetInfo().setCount);

    if (m_pDevice->MustWriteImmutableSamplers() && (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg))
    {
        Pal::PipelineBindPoint palBindPoint;
        PipelineBindPoint      apiBindPoint;
        ConvertPipelineBindPoint(pipelineBindPoint, &palBindPoint, &apiBindPoint);

        const DescriptorSetLayout*             pDestSetLayout    = pLayout->GetSetLayouts(set);
        const DescriptorSetLayout::CreateInfo& destSetLayoutInfo = pDestSetLayout->Info();
        const size_t                           descriptorSetSize = destSetLayoutInfo.sta.dwSize;
        const size_t                           alignmentInDwords =
            m_pDevice->GetProperties().descriptorSizes.alignmentInDwords;

        utils::IterateMask deviceGroup(m_curDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();
            Pal::gpusize   gpuAddr;
            uint32*        pCpuAddr  = PalCmdBuffer(deviceIdx)->CmdAllocateEmbeddedData(descriptorSetSize,
                                                                                        alignmentInDwords,
                                                                                        &gpuAddr);

            for (uint32_t bindingIndex = 0; bindingIndex < destSetLayoutInfo.count; ++bindingIndex)
            {
                const DescriptorSetLayout::BindingInfo& bindingInfo = pDestSetLayout->Binding(bindingIndex);

                // Determine whether the binding has immutable sampler descriptors.
                if (bindingInfo.imm.dwSize != 0)
                {
                    uint32_t* pSamplerDesc          = destSetLayoutInfo.imm.pImmutableSamplerData +
                                                      bindingInfo.imm.dwOffset;
                    const size_t srcArrayStrideInDW = bindingInfo.imm.dwArrayStride;
                    uint32_t numOfSamplers          = bindingInfo.info.descriptorCount;

                    for (uint32_t descriptorIdx = 0; descriptorIdx < numOfSamplers; ++descriptorIdx)
                    {
                        size_t destOffset  = pDestSetLayout->GetDstStaOffset(bindingInfo, descriptorIdx);

                        memcpy(pCpuAddr + destOffset,
                               pSamplerDesc,
                               (sizeof(uint32_t) * bindingInfo.imm.dwSize) / numOfSamplers);

                        pSamplerDesc += srcArrayStrideInDW;
                    }
                }
            }

            PerGpuState(deviceIdx)->setBindingData[apiBindPoint][setLayoutInfo.setPtrRegOffset] =
                static_cast<uint32_t>(gpuAddr);
        }
        while (deviceGroup.IterateNext());

        SetUserDataPipelineLayout(set, 1, pLayout, palBindPoint, apiBindPoint);
    }
}

// =====================================================================================================================
void CmdBuffer::ValidateGraphicsStates()
{
    if (m_allGpuState.dirtyGraphics.u32All != 0)
    {
        const DynamicDepthStencil* pDepthStencil = nullptr;
        const DynamicColorBlend*   pColorBlend   = nullptr;
        const DynamicMsaa*         pMsaa         = nullptr;

        utils::IterateMask deviceGroup(m_cbBeginDeviceMask);
        do
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            if (m_allGpuState.dirtyGraphics.colorBlend)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                RenderStateCache* pRSCache = m_pDevice->GetRenderStateCache();

                if (pColorBlend == nullptr)
                {
                    DynamicColorBlend colorBlend = {};

                    pRSCache->CreateColorBlendState(m_allGpuState.colorBlendCreateInfo,
                        m_pDevice->VkInstance()->GetAllocCallbacks(),
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                        colorBlend.pPalColorBlend);

                    // Check if pPalColorBlend is already in the m_palColorBlendState, destroy it and use the old one
                    // if yes.The destroy is not expensive since it's just a refCount--.
                    for (uint32_t i = 0; i < m_palColorBlendState.NumElements(); ++i)
                    {
                        const DynamicColorBlend& palColorBlendState = m_palColorBlendState.At(i);

                        // Check device0 only should be sufficient
                        if (palColorBlendState.pPalColorBlend[0] == colorBlend.pPalColorBlend[0])
                        {
                            pRSCache->DestroyColorBlendState(colorBlend.pPalColorBlend,
                                m_pDevice->VkInstance()->GetAllocCallbacks());

                            pColorBlend = &palColorBlendState;
                            break;
                        }
                    }

                    // Add it to the m_palColorBlendState if it doesn't exist
                    if (pColorBlend == nullptr)
                    {
                        m_palColorBlendState.PushBack(colorBlend);
                        pColorBlend = &m_palColorBlendState.Back();
                    }
                }

                VK_ASSERT(pColorBlend != nullptr);

                PalCmdBindColorBlendState(
                    m_pPalCmdBuffers[deviceIdx],
                    deviceIdx,
                    pColorBlend->pPalColorBlend[deviceIdx]);

                bool dualSourceBlendEnable = m_pDevice->PalDevice(DefaultDeviceIndex)->CanEnableDualSourceBlend(
                    m_allGpuState.colorBlendCreateInfo);

                auto pDynamicState =
                    &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
                if (dualSourceBlendEnable != pDynamicState->dualSourceBlendEnable)
                {
                    pDynamicState->dualSourceBlendEnable = dualSourceBlendEnable;
                    m_allGpuState.dirtyGraphics.pipeline = 1;
                }

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.pipeline)
            {
                const GraphicsPipeline* pGraphicsPipeline = m_allGpuState.pGraphicsPipeline;
                if (pGraphicsPipeline != nullptr)
                {
                    Pal::PipelineBindParams params = {};

                    params.pipelineBindPoint     = Pal::PipelineBindPoint::Graphics;
                    params.pPipeline             = pGraphicsPipeline->GetPalPipeline(deviceIdx);
                    params.graphics              = pGraphicsPipeline->GetBindInfo();
                    params.graphics.dynamicState =
                        m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
                    if (params.graphics.dynamicState.enable.depthClampMode &&
                        (params.graphics.dynamicState.enable.depthClipMode == false))
                    {
                        bool clipEnable = params.graphics.dynamicState.depthClampMode == Pal::DepthClampMode::_None;
                        params.graphics.dynamicState.enable.depthClipMode = true;
                        params.graphics.dynamicState.depthClipFarEnable = clipEnable;
                        params.graphics.dynamicState.depthClipNearEnable = clipEnable;
                    }

                    params.apiPsoHash            = pGraphicsPipeline->GetApiHash();

                    PalCmdBuffer(deviceIdx)->CmdBindPipeline(params);
                }
            }

            if (m_allGpuState.dirtyGraphics.viewport)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                const GraphicsPipeline* pGraphicsPipeline = m_allGpuState.pGraphicsPipeline;

                const bool isPointSizeUsed = (pGraphicsPipeline != nullptr) && pGraphicsPipeline->IsPointSizeUsed();
                Pal::ViewportParams    viewport = PerGpuState(deviceIdx)->viewport;
                if (isPointSizeUsed)
                {
                    // The default vaule is 1.0f which means the guardband is disabled.
                    // Values more than 1.0f enable guardband.
                    viewport.horzDiscardRatio = 10.0f;
                    viewport.vertDiscardRatio = 10.0f;
                }

                PalCmdBuffer(deviceIdx)->CmdSetViewports(viewport);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.scissor)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                PalCmdBuffer(deviceIdx)->CmdSetScissorRects(PerGpuState(deviceIdx)->scissor);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.rasterState)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                PalCmdBuffer(deviceIdx)->CmdSetTriangleRasterState(m_allGpuState.triangleRasterState);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.stencilRef)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                PalCmdBuffer(deviceIdx)->CmdSetStencilRefMasks(m_allGpuState.stencilRefMasks);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.inputAssembly)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                PalCmdBuffer(deviceIdx)->CmdSetInputAssemblyState(m_allGpuState.inputAssemblyState);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.vrs)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                const GraphicsPipeline* pGraphicsPipeline = m_allGpuState.pGraphicsPipeline;

                const bool force1x1 = (pGraphicsPipeline != nullptr) &&
                                      (pGraphicsPipeline->Force1x1ShaderRateEnabled());

                // CmdSetPerDrawVrsRate has been called for the dynamic state
                // Look at the currently bound pipeline and see if we need to force the values to 1x1
                Pal::VrsRateParams vrsRate = m_allGpuState.vrsRate;
                if (force1x1)
                {
                    Force1x1ShaderRate(&vrsRate);
                }

                if (m_allGpuState.minSampleShading > 0.0)
                {
                    if ((m_allGpuState.vrsRate.shadingRate == Pal::VrsShadingRate::_1x1) &&
                        (pGraphicsPipeline != nullptr) &&
                        (pGraphicsPipeline->GetPipelineFlags().shadingRateUsedInShader == false) &&
                        pGraphicsPipeline->ContainsDynamicState(DynamicStatesInternal::FragmentShadingRateStateKhr))
                    {
                        vrsRate.combinerState[static_cast<uint32>(Pal::VrsCombinerStage::PsIterSamples)] =
                            Pal::VrsCombiner::Override;
                    }
                }

                PalCmdBuffer(deviceIdx)->CmdSetPerDrawVrsRate(vrsRate);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

            if (m_allGpuState.dirtyGraphics.depthStencil)
            {
                RenderStateCache* pRSCache = m_pDevice->GetRenderStateCache();

                if (pDepthStencil == nullptr)
                {
                    DynamicDepthStencil depthStencil = {};

                    pRSCache->CreateDepthStencilState(m_allGpuState.depthStencilCreateInfo,
                                                      m_pDevice->VkInstance()->GetAllocCallbacks(),
                                                      VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                                                      depthStencil.pPalDepthStencil);

                    // Check if pPalDepthStencil is already in the m_allGpuState.palDepthStencilState, destroy it
                    // and use the old one if yes. The destroy is not expensive since it's just a refCount--.
                    for (uint32_t i = 0; i < m_palDepthStencilState.NumElements(); ++i)
                    {
                        const DynamicDepthStencil& palDepthStencilState = m_palDepthStencilState.At(i);

                        // Check device0 only should be sufficient
                        if (palDepthStencilState.pPalDepthStencil[0] == depthStencil.pPalDepthStencil[0])
                        {
                            pRSCache->DestroyDepthStencilState(depthStencil.pPalDepthStencil,
                                                               m_pDevice->VkInstance()->GetAllocCallbacks());

                            pDepthStencil = &palDepthStencilState;
                            break;
                        }
                    }

                    // Add it to the m_palDepthStencilState if it doesn't exist
                    if (pDepthStencil == nullptr)
                    {
                        m_palDepthStencilState.PushBack(depthStencil);
                        pDepthStencil = &m_palDepthStencilState.Back();
                    }
                }

                VK_ASSERT(pDepthStencil != nullptr);

                PalCmdBindDepthStencilState(
                        m_pPalCmdBuffers[deviceIdx],
                        deviceIdx,
                        pDepthStencil->pPalDepthStencil[deviceIdx]);
            }

            if (m_allGpuState.dirtyGraphics.samplePattern)
            {
                if (m_allGpuState.samplePattern.sampleCount != 0)
                {
                    PalCmdBuffer(deviceGroup.Index())->CmdSetMsaaQuadSamplePattern(
                        m_allGpuState.samplePattern.sampleCount,
                        m_allGpuState.sampleLocationsEnable ?
                            m_allGpuState.samplePattern.locations :
                            *Device::GetDefaultQuadSamplePattern(m_allGpuState.samplePattern.sampleCount));
                }
            }

            if (m_allGpuState.dirtyGraphics.msaa)
            {
                DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

                RenderStateCache* pRSCache = m_pDevice->GetRenderStateCache();

                if (pMsaa == nullptr)
                {
                    DynamicMsaa msaa = {};

                    pRSCache->CreateMsaaState(m_allGpuState.msaaCreateInfo,
                        m_pDevice->VkInstance()->GetAllocCallbacks(),
                        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                        msaa.pPalMsaa);

                    // Check if pPalMsaa is already in the m_palMsaaState, destroy it and use the old one if yes.
                    // The destroy is not expensive since it's just a refCount--.
                    for (uint32_t i = 0; i < m_palMsaaState.NumElements(); ++i)
                    {
                        const DynamicMsaa& palMsaaState = m_palMsaaState.At(i);

                        // Check device0 only should be sufficient
                        if (palMsaaState.pPalMsaa[0] == msaa.pPalMsaa[0])
                        {
                            pRSCache->DestroyMsaaState(msaa.pPalMsaa,
                                m_pDevice->VkInstance()->GetAllocCallbacks());

                            pMsaa = &palMsaaState;
                            break;
                        }
                    }

                    // Add it to the m_palMsaaState if it doesn't exist
                    if (pMsaa == nullptr)
                    {
                        m_palMsaaState.PushBack(msaa);
                        pMsaa = &m_palMsaaState.Back();
                    }
                }

                VK_ASSERT(pMsaa != nullptr);

                PalCmdBindMsaaState(
                    m_pPalCmdBuffers[deviceIdx],
                    deviceIdx,
                    pMsaa->pPalMsaa[deviceIdx]);

                DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
            }

        }
        while (deviceGroup.IterateNext());

        // Clear the dirty bits
        m_allGpuState.dirtyGraphics.u32All = 0;
    }
}

// =====================================================================================================================
void CmdBuffer::ValidateSamplePattern(
    uint32_t       sampleCount,
    SamplePattern* pSamplePattern)
{
    if (m_palQueueType == Pal::QueueTypeUniversal)
    {
        // if the current sample count is different than the current state,
        // use the sample pattern passed in or the default one
        if (sampleCount != m_allGpuState.samplePattern.sampleCount)
        {
            const Pal::MsaaQuadSamplePattern* pLocations;

            if ((pSamplePattern != nullptr) && (pSamplePattern->sampleCount > 0))
            {
                VK_ASSERT(sampleCount == pSamplePattern->sampleCount);

                PalCmdSetMsaaQuadSamplePattern(pSamplePattern->sampleCount, pSamplePattern->locations);
                pLocations = &pSamplePattern->locations;
            }
            else
            {
                pLocations = Device::GetDefaultQuadSamplePattern(sampleCount);
                PalCmdSetMsaaQuadSamplePattern(sampleCount, *pLocations);
            }

            // If the current state doesn't have a valid sample count/pattern, update to this and clear the dirty bit.
            // Otherwise, we have to assume that a draw may be issued next depending on the previous sample pattern.
            if (m_allGpuState.samplePattern.sampleCount == 0)
            {
                m_allGpuState.samplePattern.sampleCount = sampleCount;
                m_allGpuState.samplePattern.locations = *pLocations;
                m_allGpuState.dirtyGraphics.samplePattern = 0;
            }
            else
            {
                m_allGpuState.dirtyGraphics.samplePattern = 1;
            }
        }
        // set current sample pattern in the hardware if it hasn't been set yet
        else if (m_allGpuState.dirtyGraphics.samplePattern)
        {
            PalCmdSetMsaaQuadSamplePattern(
                m_allGpuState.samplePattern.sampleCount,
                m_allGpuState.sampleLocationsEnable ?
                    m_allGpuState.samplePattern.locations :
                    *Device::GetDefaultQuadSamplePattern(m_allGpuState.samplePattern.sampleCount));

            m_allGpuState.dirtyGraphics.samplePattern = 0;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetCullModeEXT(
    VkCullModeFlags cullMode)
{
    Pal::CullMode palCullMode = VkToPalCullMode(cullMode);

    if (m_allGpuState.triangleRasterState.cullMode != palCullMode)
    {
        m_allGpuState.triangleRasterState.cullMode = palCullMode;
        m_allGpuState.dirtyGraphics.rasterState            = 1;
    }

    m_allGpuState.staticTokens.triangleRasterState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetFrontFaceEXT(
    VkFrontFace frontFace)
{
    Pal::FaceOrientation palFrontFace = VkToPalFaceOrientation(frontFace);

    if (m_allGpuState.triangleRasterState.frontFace != palFrontFace)
    {
        m_allGpuState.triangleRasterState.frontFace = palFrontFace;
        m_allGpuState.dirtyGraphics.rasterState             = 1;
    }

    m_allGpuState.staticTokens.triangleRasterState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetPrimitiveTopologyEXT(
    VkPrimitiveTopology primitiveTopology)
{
    Pal::PrimitiveTopology palTopology = VkToPalPrimitiveTopology(primitiveTopology);

    if (m_allGpuState.inputAssemblyState.topology != palTopology)
    {
        m_allGpuState.inputAssemblyState.topology = palTopology;
        m_allGpuState.dirtyGraphics.inputAssembly         = 1;
    }

    m_allGpuState.staticTokens.inputAssemblyState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetDepthTestEnableEXT(
    VkBool32 depthTestEnable)
{
    if (m_allGpuState.depthStencilCreateInfo.depthEnable != static_cast<bool>(depthTestEnable))
    {
        m_allGpuState.depthStencilCreateInfo.depthEnable = depthTestEnable;
        m_allGpuState.dirtyGraphics.depthStencil                 = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthWriteEnableEXT(
    VkBool32 depthWriteEnable)
{
    if (m_allGpuState.depthStencilCreateInfo.depthWriteEnable != static_cast<bool>(depthWriteEnable))
    {
        m_allGpuState.depthStencilCreateInfo.depthWriteEnable = depthWriteEnable;
        m_allGpuState.dirtyGraphics.depthStencil                      = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthCompareOpEXT(
    VkCompareOp depthCompareOp)
{
    Pal::CompareFunc compareOp = VkToPalCompareFunc(depthCompareOp);

    if (m_allGpuState.depthStencilCreateInfo.depthFunc != compareOp)
    {
        m_allGpuState.depthStencilCreateInfo.depthFunc = compareOp;
        m_allGpuState.dirtyGraphics.depthStencil               = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthBoundsTestEnableEXT(
    VkBool32 depthBoundsTestEnable)
{
    if (m_allGpuState.depthStencilCreateInfo.depthBoundsEnable != static_cast<bool>(depthBoundsTestEnable))
    {
        m_allGpuState.depthStencilCreateInfo.depthBoundsEnable = depthBoundsTestEnable;
        m_allGpuState.dirtyGraphics.depthStencil                       = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetStencilTestEnableEXT(
    VkBool32 stencilTestEnable)
{
    if (m_allGpuState.depthStencilCreateInfo.stencilEnable != static_cast<bool>(stencilTestEnable))
    {
        m_allGpuState.depthStencilCreateInfo.stencilEnable = stencilTestEnable;
        m_allGpuState.dirtyGraphics.depthStencil                   = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetStencilOpEXT(
    VkStencilFaceFlags faceMask,
    VkStencilOp        failOp,
    VkStencilOp        passOp,
    VkStencilOp        depthFailOp,
    VkCompareOp        compareOp)
{
    Pal::StencilOp   palFailOp      = VkToPalStencilOp(failOp);
    Pal::StencilOp   palPassOp      = VkToPalStencilOp(passOp);
    Pal::StencilOp   palDepthFailOp = VkToPalStencilOp(depthFailOp);
    Pal::CompareFunc palCompareOp   = VkToPalCompareFunc(compareOp);

    Pal::DepthStencilStateCreateInfo* pCreateInfo = &(m_allGpuState.depthStencilCreateInfo);

    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        if ((pCreateInfo->front.stencilFailOp != palFailOp) ||
            (pCreateInfo->front.stencilPassOp != palPassOp) ||
            (pCreateInfo->front.stencilDepthFailOp != palDepthFailOp) ||
            (pCreateInfo->front.stencilFunc != palCompareOp))
        {
            pCreateInfo->front.stencilFailOp      = palFailOp;
            pCreateInfo->front.stencilPassOp      = palPassOp;
            pCreateInfo->front.stencilDepthFailOp = palDepthFailOp;
            pCreateInfo->front.stencilFunc        = palCompareOp;

            m_allGpuState.dirtyGraphics.depthStencil = 1;
        }
    }

    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        if ((pCreateInfo->back.stencilFailOp != palFailOp) ||
            (pCreateInfo->back.stencilPassOp != palPassOp) ||
            (pCreateInfo->back.stencilDepthFailOp != palDepthFailOp) ||
            (pCreateInfo->back.stencilFunc != palCompareOp))
        {
            pCreateInfo->back.stencilFailOp      = palFailOp;
            pCreateInfo->back.stencilPassOp      = palPassOp;
            pCreateInfo->back.stencilDepthFailOp = palDepthFailOp;
            pCreateInfo->back.stencilFunc        = palCompareOp;

            m_allGpuState.dirtyGraphics.depthStencil = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetColorWriteEnableEXT(
    uint32_t                                    attachmentCount,
    const VkBool32*                             pColorWriteEnables)
{
    if (pColorWriteEnables != nullptr)
    {
        attachmentCount = Util::Min(attachmentCount, Pal::MaxColorTargets);
        uint32_t colorWriteEnable = m_allGpuState.colorWriteEnable;
        for (uint32 i = 0; i < attachmentCount; ++i)
        {
            if (pColorWriteEnables[i])
            {
                colorWriteEnable |= (0xF << (4 * i));
            }
            else
            {
                colorWriteEnable &= ~(0xF << (4 * i));
            }
        }

        if (colorWriteEnable != m_allGpuState.colorWriteEnable)
        {
            m_allGpuState.colorWriteEnable = colorWriteEnable;
            auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
            pDynamicState->colorWriteMask = m_allGpuState.colorWriteMask & colorWriteEnable;
            if (pDynamicState->enable.colorWriteMask)
            {
                m_allGpuState.dirtyGraphics.pipeline = 1;
            }
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetRasterizerDiscardEnableEXT(
    VkBool32                                   rasterizerDiscardEnable)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    if (pDynamicState->rasterizerDiscardEnable != static_cast<bool>(rasterizerDiscardEnable))
    {
        pDynamicState->rasterizerDiscardEnable = rasterizerDiscardEnable;
        if (pDynamicState->enable.rasterizerDiscardEnable)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetPrimitiveRestartEnableEXT(
    VkBool32                                   primitiveRestartEnable)
{
    if (m_allGpuState.inputAssemblyState.primitiveRestartEnable != static_cast<bool>(primitiveRestartEnable))
    {
        m_allGpuState.inputAssemblyState.primitiveRestartEnable = primitiveRestartEnable;
        m_allGpuState.dirtyGraphics.inputAssembly                       = 1;
    }

    m_allGpuState.staticTokens.inputAssemblyState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetDepthBiasEnableEXT(
    VkBool32                                   depthBiasEnable)
{
    if ((m_allGpuState.triangleRasterState.flags.frontDepthBiasEnable != depthBiasEnable) ||
        (m_allGpuState.triangleRasterState.flags.backDepthBiasEnable  != depthBiasEnable))
    {
        m_allGpuState.triangleRasterState.flags.frontDepthBiasEnable = depthBiasEnable;
        m_allGpuState.triangleRasterState.flags.backDepthBiasEnable  = depthBiasEnable;
        m_allGpuState.dirtyGraphics.rasterState                      = 1;
    }

    m_allGpuState.staticTokens.triangleRasterState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetColorBlendEnable(
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkBool32*                     pColorBlendEnables)
{
    uint32_t lastAttachment = Util::Min(firstAttachment + attachmentCount, Pal::MaxColorTargets);
    for (uint32_t i = firstAttachment; i < lastAttachment; i++)
    {
        if (m_allGpuState.colorBlendCreateInfo.targets[i].blendEnable !=
            static_cast<bool>(pColorBlendEnables[i - firstAttachment]))
        {
            m_allGpuState.colorBlendCreateInfo.targets[i].blendEnable =
                static_cast<bool>(pColorBlendEnables[i - firstAttachment]);
            m_allGpuState.dirtyGraphics.colorBlend = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetColorBlendEquation(
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const VkColorBlendEquationEXT*      pColorBlendEquations)
{
    uint32_t lastAttachment = Util::Min(firstAttachment + attachmentCount, Pal::MaxColorTargets);

    for (uint32_t i = firstAttachment; i < lastAttachment; i++)
    {
        const VkColorBlendEquationEXT& colorBlendEquation = pColorBlendEquations[i - firstAttachment];
        auto                           pTarget            = &m_allGpuState.colorBlendCreateInfo.targets[i];

        Pal::Blend     srcBlendColor  = VkToPalBlend(colorBlendEquation.srcColorBlendFactor);
        Pal::Blend     dstBlendColor  = VkToPalBlend(colorBlendEquation.dstColorBlendFactor);
        Pal::BlendFunc blendFuncColor = VkToPalBlendFunc(colorBlendEquation.colorBlendOp);
        Pal::Blend     srcBlendAlpha  = VkToPalBlend(colorBlendEquation.srcAlphaBlendFactor);
        Pal::Blend     dstBlendAlpha  = VkToPalBlend(colorBlendEquation.dstAlphaBlendFactor);
        Pal::BlendFunc blendFuncAlpha = VkToPalBlendFunc(colorBlendEquation.alphaBlendOp);

        if ((pTarget->srcBlendColor  != srcBlendColor) ||
            (pTarget->dstBlendColor  != dstBlendColor) ||
            (pTarget->blendFuncColor != blendFuncColor) ||
            (pTarget->srcBlendAlpha  != srcBlendAlpha) ||
            (pTarget->dstBlendAlpha  != dstBlendAlpha) ||
            (pTarget->blendFuncAlpha != blendFuncAlpha))
        {
            pTarget->srcBlendColor  = srcBlendColor;
            pTarget->dstBlendColor  = dstBlendColor;
            pTarget->blendFuncColor = blendFuncColor;
            pTarget->srcBlendAlpha  = srcBlendAlpha;
            pTarget->dstBlendAlpha  = dstBlendAlpha;
            pTarget->blendFuncAlpha = blendFuncAlpha;
            m_allGpuState.dirtyGraphics.colorBlend = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetRasterizationSamples(
    VkSampleCountFlagBits               rasterizationSamples)
{
    const uint32_t rasterizationSampleCount = rasterizationSamples;

    if (rasterizationSampleCount != m_allGpuState.msaaCreateInfo.coverageSamples)
    {
        m_allGpuState.msaaCreateInfo.coverageSamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.exposedSamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.sampleClusters = rasterizationSampleCount;
        if (m_allGpuState.minSampleShading > 0.0f)
        {
            m_allGpuState.msaaCreateInfo.pixelShaderSamples =
                Pow2Pad(static_cast<uint32_t>(ceil(rasterizationSampleCount * m_allGpuState.minSampleShading)));
        }
        else
        {
            m_allGpuState.msaaCreateInfo.pixelShaderSamples = 1;
        }

        m_allGpuState.msaaCreateInfo.depthStencilSamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.shaderExportMaskSamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.alphaToCoverageSamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.occlusionQuerySamples = rasterizationSampleCount;
        m_allGpuState.msaaCreateInfo.flags.enable1xMsaaSampleLocations = (rasterizationSampleCount == 1);

        m_allGpuState.dirtyGraphics.msaa = 1;
    }

    ValidateSamplePattern(rasterizationSampleCount, nullptr);
    m_allGpuState.samplePattern.sampleCount = rasterizationSampleCount;
}

// =====================================================================================================================
void CmdBuffer::SetSampleMask(
    VkSampleCountFlagBits               samples,
    const VkSampleMask*                 pSampleMask)
{
    if (m_allGpuState.msaaCreateInfo.sampleMask != *pSampleMask)
    {
        m_allGpuState.msaaCreateInfo.sampleMask = *pSampleMask;
        m_allGpuState.dirtyGraphics.msaa = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetConservativeRasterizationMode(
    VkConservativeRasterizationModeEXT  conservativeRasterizationMode)
{
    VK_ASSERT(m_pDevice->IsExtensionEnabled(DeviceExtensions::EXT_CONSERVATIVE_RASTERIZATION));
    bool enableConservativeRasterization = false;
    Pal::ConservativeRasterizationMode conservativeMode = Pal::ConservativeRasterizationMode::Overestimate;
    switch (conservativeRasterizationMode)
    {
    case VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT:
        {
            enableConservativeRasterization = false;
        }
        break;
    case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
        {
            enableConservativeRasterization = true;
            conservativeMode = Pal::ConservativeRasterizationMode::Overestimate;
        }
        break;
    case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
        {
            enableConservativeRasterization = true;
            conservativeMode = Pal::ConservativeRasterizationMode::Underestimate;
        }
        break;
    default:
        break;
    }

    if ((m_allGpuState.msaaCreateInfo.flags.enableConservativeRasterization != enableConservativeRasterization) ||
        (enableConservativeRasterization &&
            (conservativeMode != m_allGpuState.msaaCreateInfo.conservativeRasterizationMode)))
    {
        m_allGpuState.msaaCreateInfo.flags.enableConservativeRasterization = enableConservativeRasterization;
        m_allGpuState.msaaCreateInfo.conservativeRasterizationMode         = conservativeMode;
        m_allGpuState.dirtyGraphics.msaa                                   = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetExtraPrimitiveOverestimationSize(
    float                               extraPrimitiveOverestimationSize)
{
    // Do nothing
    return;
}

// =====================================================================================================================
void CmdBuffer::SetLineStippleEnable(
    VkBool32                            stippledLineEnable)
{
    if (m_allGpuState.msaaCreateInfo.flags.enableLineStipple != stippledLineEnable)
    {
        m_allGpuState.msaaCreateInfo.flags.enableLineStipple = stippledLineEnable;
        m_allGpuState.dirtyGraphics.msaa                     = 1;
    }
}

// =====================================================================================================================
void CmdBuffer::SetPolygonMode(
    VkPolygonMode                       polygonMode)
{
    Pal::FillMode fillMode = VkToPalFillMode(polygonMode);
    if (m_allGpuState.triangleRasterState.frontFillMode != fillMode)
    {
        m_allGpuState.triangleRasterState.frontFillMode = fillMode;
        m_allGpuState.triangleRasterState.backFillMode = fillMode;
        m_allGpuState.dirtyGraphics.rasterState = 1;
    }

    m_allGpuState.staticTokens.triangleRasterState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetProvokingVertexMode(
    VkProvokingVertexModeEXT            provokingVertexMode)
{
    Pal::ProvokingVertex provokingVertex = VkToPalProvokingVertex(provokingVertexMode);
    if (m_allGpuState.triangleRasterState.provokingVertex != provokingVertex)
    {
        m_allGpuState.triangleRasterState.provokingVertex = provokingVertex;
        m_allGpuState.dirtyGraphics.rasterState = 1;
    }

    m_allGpuState.staticTokens.triangleRasterState = DynamicRenderStateToken;
}

// =====================================================================================================================
void CmdBuffer::SetColorWriteMask(
    uint32_t                            firstAttachment,
    uint32_t                            attachmentCount,
    const  VkColorComponentFlags*       pColorWriteMasks)
{
    uint32_t lastAttachment = Util::Min(firstAttachment + attachmentCount, Pal::MaxColorTargets);
    uint32_t colorWriteMask = m_allGpuState.colorWriteMask;
    for (uint32_t i = firstAttachment; i < lastAttachment; i++)
    {
        colorWriteMask &= ~(0xF << (4 * i));
        colorWriteMask |= pColorWriteMasks[i - firstAttachment] << (4 * i);
    }

    if (colorWriteMask != m_allGpuState.colorWriteMask)
    {
        m_allGpuState.colorWriteMask = colorWriteMask;
        auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
        pDynamicState->colorWriteMask = colorWriteMask & m_allGpuState.colorWriteEnable;
        if (pDynamicState->enable.colorWriteMask)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetSampleLocationsEnable(
    VkBool32                            sampleLocationsEnable)
{
    if (m_allGpuState.sampleLocationsEnable != sampleLocationsEnable)
    {
        m_allGpuState.sampleLocationsEnable = sampleLocationsEnable;
        m_allGpuState.dirtyGraphics.samplePattern = 1;
    }
}
// =====================================================================================================================
void CmdBuffer::SetLineRasterizationMode(
    VkLineRasterizationModeEXT          lineRasterizationMode)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    bool perpLineEndCapsEnable = lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT;
    if (perpLineEndCapsEnable != pDynamicState->perpLineEndCapsEnable)
    {
        pDynamicState->perpLineEndCapsEnable = perpLineEndCapsEnable;

        if (pDynamicState->enable.perpLineEndCapsEnable)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetLogicOp(
    VkLogicOp                           logicOp)
{
    if (m_allGpuState.logicOp != logicOp)
    {
        m_allGpuState.logicOp = logicOp;
        if (m_allGpuState.logicOpEnable)
        {
            auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
            pDynamicState->logicOp = VkToPalLogicOp(logicOp);
            if (pDynamicState->enable.logicOp)
            {
                m_allGpuState.dirtyGraphics.pipeline = 1;
            }
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetLogicOpEnable(
    VkBool32                            logicOpEnable)
{
    if (m_allGpuState.logicOpEnable != logicOpEnable)
    {
        m_allGpuState.logicOpEnable = logicOpEnable;
        auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
        pDynamicState->logicOp =
            m_allGpuState.logicOpEnable ? VkToPalLogicOp(m_allGpuState.logicOp) : Pal::LogicOp::Copy;
        if (pDynamicState->enable.logicOp)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetTessellationDomainOrigin(
    VkTessellationDomainOrigin          domainOrigin)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    bool switchWinding = domainOrigin == VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT;
    if (switchWinding != pDynamicState->switchWinding)
    {
        pDynamicState->switchWinding = switchWinding;

        if (pDynamicState->enable.switchWinding)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthClampEnable(
    VkBool32                            depthClampEnable)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    Pal::DepthClampMode clampMode = depthClampEnable ? Pal::DepthClampMode::Viewport : Pal::DepthClampMode::_None;
    if (clampMode != pDynamicState->depthClampMode)
    {
        pDynamicState->depthClampMode = clampMode;
        if (pDynamicState->enable.depthClampMode)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetAlphaToCoverageEnable(
    VkBool32                            alphaToCoverageEnable)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;

    if (static_cast<bool>(alphaToCoverageEnable) != pDynamicState->alphaToCoverageEnable)
    {
        pDynamicState->alphaToCoverageEnable = alphaToCoverageEnable;

        if (pDynamicState->enable.alphaToCoverageEnable)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthClipEnable(
    VkBool32                            depthClipEnable)
{
    auto pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    if (static_cast<bool>(depthClipEnable) != pDynamicState->depthClipNearEnable)
    {
        pDynamicState->depthClipNearEnable = depthClipEnable;
        pDynamicState->depthClipFarEnable  = depthClipEnable;

        if (pDynamicState->enable.depthClipMode)
        {
            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
void CmdBuffer::SetDepthClipNegativeOneToOne(
    VkBool32                            negativeOneToOne)
{
    auto         pDynamicState = &m_allGpuState.pipelineState[PipelineBindGraphics].dynamicBindInfo.gfx.dynamicState;
    Pal::DepthRange depthRange = negativeOneToOne ? Pal::DepthRange::NegativeOneToOne : Pal::DepthRange::ZeroToOne;
    if (depthRange != pDynamicState->depthRange)
    {
        pDynamicState->depthRange = depthRange;

        if (pDynamicState->enable.depthRange)
        {
            utils::IterateMask deviceGroup(m_curDeviceMask);
            do
            {
                PerGpuState(deviceGroup.Index())->viewport.depthRange = depthRange;
            }
            while (deviceGroup.IterateNext());

            m_allGpuState.dirtyGraphics.viewport = 1;
            m_allGpuState.staticTokens.viewports = DynamicRenderStateToken;

            m_allGpuState.dirtyGraphics.pipeline = 1;
        }
    }
}

// =====================================================================================================================
RenderPassInstanceState::RenderPassInstanceState(
    PalAllocator* pAllocator)
    :
    pExecuteInfo(nullptr),
    subpass(VK_SUBPASS_EXTERNAL),
    renderAreaCount(0),
    maxAttachmentCount(0),
    pAttachments(nullptr),
    maxSubpassCount(0),
    pSamplePatterns(nullptr)
{
    memset(&renderArea[0], 0, sizeof(renderArea));
}

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

template
void CmdBuffer::DrawIndirect<false, false>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::DrawIndirect<true, false>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::DrawIndirect<false, true>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::DrawIndirect<true, true>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::DrawMeshTasksIndirect<false>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::DrawMeshTasksIndirect<true>(
    VkBuffer        buffer,
    VkDeviceSize    offset,
    uint32_t        count,
    uint32_t        stride,
    VkBuffer        countBuffer,
    VkDeviceSize    countOffset);

template
void CmdBuffer::ResolveImage<VkImageResolve>(
    VkImage                 srcImage,
    VkImageLayout           srcImageLayout,
    VkImage                 destImage,
    VkImageLayout           destImageLayout,
    uint32_t                rectCount,
    const VkImageResolve*   pRects);

template
void CmdBuffer::ResolveImage<VkImageResolve2>(
    VkImage                 srcImage,
    VkImageLayout           srcImageLayout,
    VkImage                 destImage,
    VkImageLayout           destImageLayout,
    uint32_t                rectCount,
    const VkImageResolve2*  pRects);

} // namespace vk
