/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_object.h"
#include "include/vk_utils.h"
#include "include/vk_query.h"
#include "include/vk_queue.h"
#include "include/peer_resource.h"

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

#include <float.h>

namespace vk
{
namespace
{

// =====================================================================================================================
// This finds the subset of an images subres ranges that need to be transitioned based changes between the source and
// destination layouts.  In the event that no layout transitions are required, a single transition is still returned
// to handle cache syncs.
void FindDepthStencilLayoutTransitionRanges(
    const uint32_t oldLayouts[MaxRangePerAttachment],
    const uint32_t newLayouts[MaxRangePerAttachment],
    uint32_t*      pStartRange,
    uint32_t*      pNumRangeTransitions)
{
    // Assume the default case that both transitions are required.
    uint32_t startRange = 0;
    uint32_t numTransitions = MaxRangePerAttachment;

    if (oldLayouts[0] == newLayouts[0])
    {
        // Skip the depth transition
        numTransitions--;

        startRange++;
    }
    else if (oldLayouts[1] == newLayouts[1])
    {
        // Skip the stencil transition
        numTransitions--;
    }

    *pStartRange = startRange;
    *pNumRangeTransitions = numTransitions;
}

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
    const RenderPass&           renderPass,
    const uint32_t              subpass,
    PalClearRegionVect* const   pOutClearRegions)
{
    using ClearRegionType = typename std::remove_pointer<decltype(pOutClearRegions->Data())>::type;

    static_assert(std::is_same<ClearRegionType, Pal::ClearBoundTargetRegion>::value ||
                  std::is_same<ClearRegionType, Pal::Box>::value, "Wrong element type");
    VK_ASSERT(pOutClearRegions != nullptr);

    Pal::Result palResult = Pal::Result::Success;

    pOutClearRegions->Clear();

    if (renderPass.IsMultiviewEnabled())
    {
        const auto viewMask    = renderPass.GetViewMask(subpass);
        const auto layerRanges = RangesOfOnesInBitMask(viewMask);

        palResult = pOutClearRegions->Reserve(rectCount * layerRanges.NumElements());

        if (palResult == Pal::Result::Success)
        {
            for (auto layerRangeIt = layerRanges.Begin(); layerRangeIt.IsValid(); layerRangeIt.Next())
            {
                for (uint32_t rectIndex = 0; rectIndex < rectCount; ++rectIndex)
                {
                    pOutClearRegions->PushBack(VkToPalClearRegion<ClearRegionType>(pRects[rectIndex]));
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
                pOutClearRegions->PushBack(VkToPalClearRegion<ClearRegionType>(pRects[rectIndex]));
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
Util::Vector<Pal::SubresRange, MaxRangePerAttachment * Pal::MaxViewInstanceCount, Util::GenericAllocator>
LoadOpClearSubresRanges(
    const Framebuffer::Attachment& attachment,
    const RPLoadOpClearInfo&       clearInfo,
    const RenderPass&              renderPass)
{
    // Note that no allocation will be performed, so Util::Vector allocator is nullptr.
    Util::Vector<Pal::SubresRange, MaxRangePerAttachment * Pal::MaxViewInstanceCount, Util::GenericAllocator> clearSubresRanges { nullptr };

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
    Device*  pDevice,
    CmdPool* pCmdPool,
    uint32_t queueFamilyIndex)
    :
    m_pDevice(pDevice),
    m_pCmdPool(pCmdPool),
    m_queueFamilyIndex(queueFamilyIndex),
    m_palQueueType(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalQueueType(queueFamilyIndex)),
    m_palEngineType(pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetQueueFamilyPalEngineType(queueFamilyIndex)),
    m_palDeviceMask(0),
    m_palDeviceUsedMask(0),
    m_pStackAllocator(nullptr),
    m_pGpuEventMgr(nullptr),
    m_vbMgr(pDevice),
    m_is2ndLvl(false),
    m_isRecording(false),
    m_needResetState(true),
    m_recordingResult(VK_SUCCESS),
    m_pSqttState(nullptr),
    m_renderPassInstance(pDevice->VkInstance()->Allocator())
{

#if VK_ENABLE_DEBUG_BARRIERS
    m_dbgBarrierPreCmdMask  = m_pDevice->GetRuntimeSettings().dbgBarrierPreCmdEnable;
    m_dbgBarrierPostCmdMask = m_pDevice->GetRuntimeSettings().dbgBarrierPostCmdEnable;
#endif
}

// =====================================================================================================================
// Creates a new Vulkan Command Buffer object
VkResult CmdBuffer::Create(
    Device*                            pDevice,
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer*                   pCommandBuffers)
{
    union
    {
        const VkStructHeader*              pHeader;
        const VkCommandBufferAllocateInfo* pAllocInfo;
    };

    // Get information about the Vulkan command buffer
    Pal::CmdBufferCreateInfo palCreateInfo = {};
    CmdPool* pCmdPool = nullptr;
    uint32_t queueFamilyIndex = 0;
    uint32_t commandBufferCount = 0;

    for (pAllocInfo = pAllocateInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
        case VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO:
            {
                pCmdPool = CmdPool::ObjectFromHandle(pAllocateInfo->commandPool);
                queueFamilyIndex            = pCmdPool->GetQueueFamilyIndex();
                commandBufferCount          = pAllocInfo->commandBufferCount;
                palCreateInfo.pCmdAllocator = pCmdPool->PalCmdAllocator(DefaultDeviceIndex);
                palCreateInfo.queueType     = pDevice->GetQueueFamilyPalQueueType(queueFamilyIndex);
                palCreateInfo.engineType    = pDevice->GetQueueFamilyPalEngineType(queueFamilyIndex);
                palCreateInfo.flags.nested  = (pAllocInfo->level > VK_COMMAND_BUFFER_LEVEL_PRIMARY) ? 1 : 0;
            }
            break;

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    // Allocate system memory for the command buffer objects
    Pal::Result palResult;

    const uint32_t numGroupedCmdBuffers = pDevice->NumPalDevices();
    const size_t apiSize    = sizeof(ApiCmdBuffer);
    const size_t palSize    = pDevice->PalDevice(DefaultDeviceIndex)->
                                        GetCmdBufferSize(palCreateInfo, &palResult) * numGroupedCmdBuffers;
    const size_t vbSize     = VertBufBindingMgr::GetSize(pDevice) * numGroupedCmdBuffers;
    const size_t cmdBufSize = apiSize + palSize + vbSize;

    VK_ASSERT(palResult == Pal::Result::Success);

    VkResult result = VK_SUCCESS;
    Instance* pInstance = pDevice->VkInstance();

    uint32_t allocCount = 0;

    while ((result == VK_SUCCESS) && (allocCount < commandBufferCount))
    {
        // Allocate memory for the command buffer
        void* pMemory = pInstance->AllocMem(cmdBufSize, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        // Create the command buffer
        if (pMemory != nullptr)
        {
            void* pPalMem = Util::VoidPtrInc(pMemory, apiSize);
            void* pVbMem  = Util::VoidPtrInc(pPalMem, palSize);

            VK_INIT_API_OBJECT(CmdBuffer, pMemory, (pDevice, pCmdPool, queueFamilyIndex));

            pCommandBuffers[allocCount] = reinterpret_cast<VkCommandBuffer>(pMemory);

            CmdBuffer* pCmdBuffer = ApiCmdBuffer::ObjectFromHandle(pCommandBuffers[allocCount]);

            result = pCmdBuffer->Initialize(pPalMem, pVbMem, palCreateInfo);

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
    void*                           pVbMem,
    const Pal::CmdBufferCreateInfo& createInfo)
{
    Pal::Result result = Pal::Result::Success;

    Pal::CmdBufferCreateInfo groupCreateInfo = createInfo;

    // Create the PAL command buffers
    size_t       palMemOffset = 0;
    const size_t palSize      = m_pDevice->PalDevice(DefaultDeviceIndex)->GetCmdBufferSize(groupCreateInfo, &result);

    const uint32_t numGroupedCmdBuffers = m_pDevice->NumPalDevices();

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
        result = m_vbMgr.Initialize(this, pVbMem);
    }

    if (result == Pal::Result::Success)
    {
        // Register this command buffer with the pool
        result = m_pCmdPool->RegisterCmdBuffer(this);
    }

    if (result == Pal::Result::Success)
    {
        m_is2ndLvl = (groupCreateInfo.flags.nested == 1) ? true : false;
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

    return PalToVkResult(result);
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferBegin(const Pal::CmdBufferBuildInfo& cmdInfo)
{
    Pal::Result result = Pal::Result::Success;

    utils::IterateMask deviceGroup(m_palDeviceUsedMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        result = PalCmdBuffer(deviceIdx)->Begin(cmdInfo);

        VK_ASSERT(result == Pal::Result::Success);
    }

    return result;
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferEnd()
{
    Pal::Result result = Pal::Result::Success;

    utils::IterateMask deviceGroup(m_palDeviceUsedMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        result = PalCmdBuffer(deviceIdx)->End();

        VK_ASSERT(result == Pal::Result::Success);
    }

    return result;
}

// =====================================================================================================================
Pal::Result CmdBuffer::PalCmdBufferReset(Pal::ICmdAllocator* pCmdAllocator, bool returnGpuMemory)
{
    Pal::Result result = Pal::Result::Success;

    utils::IterateMask deviceGroup(m_palDeviceUsedMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        result = PalCmdBuffer(deviceIdx)->Reset(nullptr, returnGpuMemory);
        VK_ASSERT(result == Pal::Result::Success);
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::PalCmdBufferDestroy()
{
    for (uint32_t deviceIdx = 0; deviceIdx < VkDevice()->NumPalDevices(); deviceIdx++)
    {
        PalCmdBuffer(deviceIdx)->Destroy();
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindIndexData(Buffer* pBuffer, Pal::gpusize offset, Pal::IndexType indexType)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        const Pal::gpusize gpuVirtAddr = pBuffer->GpuVirtAddr(deviceIdx) + offset;

        PalCmdBuffer(deviceIdx)->CmdBindIndexData(gpuVirtAddr,
            utils::BufferSizeToIndexCount(indexType, pBuffer->GetSize()),
            indexType);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdUnbindIndexData(Pal::IndexType indexType)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdBindIndexData(0, 0, indexType);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdDraw(
    uint32_t firstVertex,
    uint32_t vertexCount,
    uint32_t firstInstance,
    uint32_t instanceCount)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdDraw(firstVertex,
            vertexCount,
            firstInstance,
            instanceCount);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdDrawIndexed(
    uint32_t firstIndex,
    uint32_t indexCount,
    int32_t  vertexOffset,
    uint32_t firstInstance,
    uint32_t instanceCount)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdDrawIndexed(firstIndex,
            indexCount,
            vertexOffset,
            firstInstance,
            instanceCount);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdDispatch(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdDispatch(x, y, z);
    }
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
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdDispatchOffset(base_x, base_y, base_z, size_x, size_y, size_z);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdDispatchIndirect(
    Buffer*      pBuffer,
    Pal::gpusize offset)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        // TODO use device group dispatch offsets here.
        // Note: check spec to see if offset setting is applications' responsibility.

        PalCmdBuffer(deviceIdx)->CmdDispatchIndirect(
            *pBuffer->PalMemory(deviceIdx),
            pBuffer->MemOffset() + offset);
    }
}

// =====================================================================================================================
// Helper function used to assert when applications p2p-write to a memory address more than once.
template< typename Type_T >
bool CmdBuffer::DetectCopyOverwrite(
    const Type_T*   pDst) const
{
    uint32_t multipleWrites = 0;

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t bitMask = 1 << pDst->GetMemoryInstanceIdx(deviceGroup.Index());

        if ((multipleWrites & bitMask) != 0)
        {
            return true;
        }

        multipleWrites |= bitMask;
    }

    return false;
}

// =====================================================================================================================
void CmdBuffer::PalCmdCopyBuffer(
    Buffer*                pSrcBuffer,
    Buffer*                pDstBuffer,
    uint32_t               regionCount,
    Pal::MemoryCopyRegion* pRegions)
{
    if (m_pDevice->IsMultiGpu() == false)  // TODO: SWDEV-120909 - Remove looping and branching where necessary
    {
        Pal::IGpuMemory* const pSrcMemory = pSrcBuffer->PalMemory();
        Pal::IGpuMemory* const pDstMemory = pDstBuffer->PalMemory();
        VK_ASSERT(pSrcMemory != nullptr);
        VK_ASSERT(pDstMemory != nullptr);

        PalCmdBuffer()->CmdCopyMemory(
            *pSrcMemory,
            *pDstMemory,
            regionCount,
            pRegions);
    }
    else
    {
        VK_ASSERT(DetectCopyOverwrite(pDstBuffer) == false);

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyMemory(
                *pSrcBuffer->PalMemory(deviceIdx),
                *pDstBuffer->PalMemory(deviceIdx),
                regionCount,
                pRegions);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdUpdateBuffer(
    Buffer*         pDestBuffer,
    Pal::gpusize    offset,
    Pal::gpusize    size,
    const uint32_t* pData)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdUpdateMemory(*pDestBuffer->PalMemory(deviceIdx), offset, size, pData);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdFillBuffer(
    Buffer*         pDestBuffer,
    Pal::gpusize    offset,
    Pal::gpusize    size,
    uint32_t        data)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdFillMemory(*pDestBuffer->PalMemory(deviceIdx), offset, size, data);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdCopyImage(
    const Image* const    pSrcImage,
    Pal::ImageLayout      srcImageLayout,
    const Image* const    pDstImage,
    Pal::ImageLayout      destImageLayout,
    uint32_t              regionCount,
    Pal::ImageCopyRegion* pRegions)
{
    if (m_pDevice->IsMultiGpu() == false)  // TODO: SWDEV-120909 - Remove looping and branching where necessary
    {
        PalCmdBuffer()->CmdCopyImage(
            *pSrcImage->PalImage(),
            srcImageLayout,
            *pDstImage->PalImage(),
            destImageLayout,
            regionCount,
            pRegions,
            0);
    }
    else
    {
        VK_ASSERT(DetectCopyOverwrite(pDstImage) == false);

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyImage(
                *pSrcImage->PalImage(deviceIdx),
                srcImageLayout,
                *pDstImage->PalImage(deviceIdx),
                destImageLayout,
                regionCount,
                pRegions,
                0);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdScaledCopyImage(
    const Image* const   pSrcImage,
    const Image* const   pDstImage,
    Pal::ScaledCopyInfo& copyInfo)
{
    if (m_pDevice->IsMultiGpu() == false)  // TODO: SWDEV-120909 - Remove looping and branching where necessary
    {
        copyInfo.pSrcImage = pSrcImage->PalImage();
        copyInfo.pDstImage = pDstImage->PalImage();

        // This will do a scaled blit
        PalCmdBuffer()->CmdScaledCopyImage(copyInfo);
    }
    else
    {
        VK_ASSERT(DetectCopyOverwrite(pDstImage) == false);

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            copyInfo.pSrcImage = pSrcImage->PalImage(deviceIdx);
            copyInfo.pDstImage = pDstImage->PalImage(deviceIdx);

            // This will do a scaled blit
            PalCmdBuffer(deviceIdx)->CmdScaledCopyImage(copyInfo);
        }
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
    if (m_pDevice->IsMultiGpu() == false)  // TODO: SWDEV-120909 - Remove looping and branching where necessary
    {
        PalCmdBuffer()->CmdCopyMemoryToImage(
            *pSrcBuffer->PalMemory(),
            *pDstImage->PalImage(),
            layout,
            regionCount,
            pRegions);
    }
    else
    {
        VK_ASSERT(DetectCopyOverwrite(pDstImage) == false);

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyMemoryToImage(
                *pSrcBuffer->PalMemory(deviceIdx),
                *pDstImage->PalImage(deviceIdx),
                layout,
                regionCount,
                pRegions);
        }
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
    if (m_pDevice->IsMultiGpu() == false)  // TODO: SWDEV-120909 - Remove looping and branching where necessary
    {
        PalCmdBuffer()->CmdCopyImageToMemory(
            *pSrcImage->PalImage(),
            layout,
            *pDstBuffer->PalMemory(),
            regionCount,
            pRegions);
    }
    else
    {
        VK_ASSERT(DetectCopyOverwrite(pDstBuffer) == false);

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdCopyImageToMemory(
                *pSrcImage->PalImage(deviceIdx),
                layout,
                *pDstBuffer->PalMemory(deviceIdx),
                regionCount,
                pRegions);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdSetIndirectUserDataWatermark(
    uint16_t      tableId,
    uint32_t      dwordLimit)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdSetIndirectUserDataWatermark(tableId, dwordLimit);
    }
}

// =====================================================================================================================
// Begin Vulkan command buffer
VkResult CmdBuffer::Begin(
    const VkCommandBufferBeginInfo* pBeginInfo)
{
    VK_ASSERT(!m_isRecording);

    // Beginning a command buffer implicitly resets its state if it is not reset before.
    if (m_needResetState)
    {
        ResetState();
    }
    else
    {
        m_needResetState = true;
    }

    Pal::CmdBufferBuildInfo   cmdInfo = { 0 };
    Pal::CmdBufferBuildFlags& palFlags = cmdInfo.flags;

    union
    {
        const VkStructHeader*                         pHeader;
        const VkCommandBufferBeginInfo*               pInfo;
        const VkDeviceGroupCommandBufferBeginInfoKHX* pDeviceGroupInfo;
    };

    RenderPass*  pRenderPass = nullptr;
    Framebuffer* pFramebuffer = nullptr;

    m_palDeviceUsedMask = m_pDevice->GetPalDeviceMask();

    palFlags.u32All = 0;
    palFlags.prefetchCommands = 1;

    if (m_pDevice->GetRuntimeSettings().prefetchShaders)
    {
        palFlags.prefetchShaders = 1;
    }

    uint32_t currentSubPass = 0;
    for (pInfo = pBeginInfo; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
            // Convert Vulkan flags to PAL flags.
        case VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO:
            palFlags.optimizeOneTimeSubmit = (pInfo->flags & VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) ? 1 : 0;
            palFlags.optimizeExclusiveSubmit = (pInfo->flags & VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT) ? 0 : 1;

            switch (m_pDevice->GetRuntimeSettings().optimizeCmdbufMode)
            {
            case EnableOptimizeForRenderPassContinue:
                palFlags.optimizeGpuSmallBatch = (pInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) ? 1 : 0;
                break;
            case EnableOptimizeCmdbuf:
                palFlags.optimizeGpuSmallBatch = 1;
                break;
            case DisableOptimizeCmdbuf:
                palFlags.optimizeGpuSmallBatch = 0;
                break;
            default:
                palFlags.optimizeGpuSmallBatch = (pInfo->flags & VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT) ? 1 : 0;
                break;
            }

            if (pInfo->pInheritanceInfo != nullptr)
            {
                pRenderPass = RenderPass::ObjectFromHandle(pInfo->pInheritanceInfo->renderPass);
                pFramebuffer = Framebuffer::ObjectFromHandle(pInfo->pInheritanceInfo->framebuffer);
                currentSubPass = pInfo->pInheritanceInfo->subpass;
            }
            break;

        case VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO_KHX:
        {
            // Check that the application did not set any bits outside of our device group mask.
            VK_ASSERT((m_palDeviceUsedMask & pDeviceGroupInfo->deviceMask) == pDeviceGroupInfo->deviceMask);

            m_palDeviceUsedMask &= pDeviceGroupInfo->deviceMask;
            break;
        }

        default:
            // Skip any unknown extension structures
            break;
        }
    }

    m_palDeviceMask = m_palDeviceUsedMask;

    Pal::InheritedStateParams inheritedStateParams = {};

    if (m_is2ndLvl && pRenderPass) // secondary VkCommandBuffer will be used inside VkRenderPass
    {
        inheritedStateParams.colorTargetCount = pRenderPass->GetSubpassColorReferenceCount(currentSubPass);
        inheritedStateParams.stateFlags.targetViewState = 1;

        for (uint32_t i = 0; i < inheritedStateParams.colorTargetCount; i++)
        {
            inheritedStateParams.colorTargetSwizzledFormats[i] =
                VkToPalFormat(pRenderPass->GetColorAttachmentFormat(currentSubPass, i));
            inheritedStateParams.sampleCount[i] = pRenderPass->GetColorAttachmentSamples(currentSubPass, i);
        }

        // only provide valid inherited state pointer for 2nd level command buffer. Right now we unconditionally
        // do this. Later when we allow to bind fbo at beinCmdbuffer() time, we can skip this
        cmdInfo.pInheritedState = &inheritedStateParams;
    }

    Pal::Result result = PalCmdBufferBegin(cmdInfo);

    DbgBarrierPreCmd(DbgBarrierCmdBufStart);

    VK_ASSERT(result == Pal::Result::Success);

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->Begin(pBeginInfo);
    }

    if (result == Pal::Result::Success)
    {
        VK_ASSERT(m_pStackAllocator == nullptr);

        result = m_pDevice->VkInstance()->StackMgr()->AcquireAllocator(&m_pStackAllocator);
    }

    if (result == Pal::Result::Success)
    {
        // If we have to resume an already started render pass then we have to do it here
        if (pRenderPass != nullptr)
        {
            m_state.allGpuState.pRenderPass = pRenderPass;

            m_renderPassInstance.subpass = currentSubPass;
        }

        // if input frame buffer object pointer is NULL, it means
        // either this is for a primary command buffer, or this is a secondary command buffer
        // and the command buffer will get the frame buffer object and execution time from
        // beginRenderPass called in the primary command buffer
        if (pFramebuffer != nullptr)
        {
            m_state.allGpuState.pFramebuffer = pFramebuffer;
        }
    }

    // Get a GPU event manager if we don't already have one
    if (m_pGpuEventMgr == nullptr)
    {
        m_pGpuEventMgr = m_pCmdPool->AcquireGpuEventMgr();

        if (m_pGpuEventMgr == nullptr)
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    // Notify the GPU event manager we're starting a new command buffer
    if (m_pGpuEventMgr != nullptr)
    {
        m_pGpuEventMgr->BeginCmdBuf(this, cmdInfo);

        m_isRecording = true;
    }

    if (m_is2ndLvl && pRenderPass) // secondary VkCommandBuffer will be used inside VkRenderPass
    {
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
        SetViewInstanceMask();
    }

    DbgBarrierPostCmd(DbgBarrierCmdBufStart);

    return PalToVkResult(result);
}

// =====================================================================================================================
// End Vulkan command buffer
VkResult CmdBuffer::End(void)
{
    Pal::Result result;

    VK_ASSERT(m_isRecording);

    DbgBarrierPreCmd(DbgBarrierCmdBufEnd);

    if (m_renderPassInstance.pAttachments != nullptr)
    {
        m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pAttachments);

        m_renderPassInstance.pAttachments       = nullptr;
        m_renderPassInstance.maxAttachmentCount = 0;
    }

    if (m_renderPassInstance.pSamplePatterns != nullptr)
    {
        m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pSamplePatterns);

        m_renderPassInstance.pSamplePatterns = nullptr;
        m_renderPassInstance.maxSubpassCount = 0;
    }

    if (m_pSqttState != nullptr)
    {
        m_pSqttState->End();
    }

    DbgBarrierPostCmd(DbgBarrierCmdBufEnd);

    result = PalCmdBufferEnd();

    if (m_pStackAllocator != nullptr)
    {
        m_pDevice->VkInstance()->StackMgr()->ReleaseAllocator(m_pStackAllocator);

        m_pStackAllocator = nullptr;
    }

    m_isRecording = false;

    return (m_recordingResult == VK_SUCCESS ? PalToVkResult(result) : m_recordingResult);
}

// =====================================================================================================================
// Resets all state except for the PAL command buffer state.  This function is called both during vkBeginCommandBuffer
// and during vkResetCommandBuffer
void CmdBuffer::ResetState()
{
    m_stencilCombiner.Reset();

    // Memset the first section of m_state.allGpuState.  The second section begins with pipelineState.
    const size_t memsetBytes = offsetof(AllGpuRenderState, pipelineState);
    memset(&m_state.allGpuState, 0, memsetBytes);

    // Reset initial static values to "dynamic" values.  This will skip initial redundancy checking because the
    // prior values are unknown.  Since DynamicRenderStateToken is 0, this is covered by the memset above.
    static_assert(DynamicRenderStateToken == 0, "Unexpected value!");

    uint32_t bindIdx = 0;
    do
    {
        memset(&(m_state.allGpuState.pipelineState[bindIdx].userDataLayout),
               0,
               sizeof(m_state.allGpuState.pipelineState[bindIdx].userDataLayout));

        m_state.allGpuState.pipelineState[bindIdx].pLayout = nullptr;
        m_state.allGpuState.pipelineState[bindIdx].boundSetCount = 0;
        m_state.allGpuState.pipelineState[bindIdx].pushedConstCount = 0;

        bindIdx++;
    }
    while (bindIdx < static_cast<uint32_t>(Pal::PipelineBindPoint::Count));

    m_state.allGpuState.scissor.count = 0;

    m_state.allGpuState.viewport.count            = 0;
    m_state.allGpuState.viewport.horzClipRatio    = FLT_MAX;
    m_state.allGpuState.viewport.vertClipRatio    = FLT_MAX;
    m_state.allGpuState.viewport.horzDiscardRatio = 1.0f;
    m_state.allGpuState.viewport.vertDiscardRatio = 1.0f;

    const uint32_t numPalDevices = m_pDevice->NumPalDevices();
    uint32_t deviceIdx = 0;
    do
    {
        m_state.perGpuState[deviceIdx].pMsaaState           = nullptr;
        m_state.perGpuState[deviceIdx].pColorBlendState     = nullptr;
        m_state.perGpuState[deviceIdx].pDepthStencilState   = nullptr;

        deviceIdx++;
    }
    while (deviceIdx < numPalDevices);

    m_palDeviceMask = InvalidPalDeviceMask;

    if (m_pGpuEventMgr != nullptr)
    {
        m_pGpuEventMgr->ResetCmdBuf(this);
    }

    m_renderPassInstance.pExecuteInfo = nullptr;
    m_renderPassInstance.subpass      = VK_SUBPASS_EXTERNAL;
    m_renderPassInstance.flags.u32All = 0;

    m_recordingResult = VK_SUCCESS;
}

// =====================================================================================================================
// Reset Vulkan command buffer
VkResult CmdBuffer::Reset(VkCommandBufferResetFlags flags)
{
    VkResult result = VK_SUCCESS;

    ResetState();

    m_needResetState = false;

    const bool releaseResources = ((flags & VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT) != 0);

    // If the command buffer is being recorded, the stack allocator will still be around.
    // Make sure to free it.
    if (m_isRecording)
    {
        End();

        VK_ASSERT(!m_isRecording);
    }

    if (releaseResources)
    {
        // Release per-attachment render pass instance memory
        if (m_renderPassInstance.pAttachments != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pAttachments);

            m_renderPassInstance.pAttachments       = nullptr;
            m_renderPassInstance.maxAttachmentCount = 0;
        }

        // Release per-subpass instance memory
        if (m_renderPassInstance.pSamplePatterns != nullptr)
        {
            m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pSamplePatterns);

            m_renderPassInstance.pSamplePatterns = nullptr;
            m_renderPassInstance.maxSubpassCount = 0;
        }

        // Release the GPU event manager back to the command pool
        if (m_pGpuEventMgr != nullptr)
        {
            m_pCmdPool->ReleaseGpuEventMgr(m_pGpuEventMgr);
            m_pGpuEventMgr = nullptr;
        }
    }

    result = PalToVkResult(PalCmdBufferReset(nullptr, releaseResources));

    return result;
}
// =====================================================================================================================
// Bind pipeline to command buffer
void CmdBuffer::BindPipeline(
    VkPipelineBindPoint     pipelineBindPoint,
    VkPipeline              pipeline)
{
    DbgBarrierPreCmd(DbgBarrierBindPipeline);

    static_assert(VK_PIPELINE_BIND_POINT_RANGE_SIZE == 2, "New pipeline bind point added");

    uint32_t bindPoint = 0;
    const PipelineLayout* pNewLayout = nullptr;

    switch (static_cast<int32_t>(pipelineBindPoint))
    {
    case VK_PIPELINE_BIND_POINT_COMPUTE:
        {
            bindPoint = static_cast<uint32_t>(Pal::PipelineBindPoint::Compute);

            if (pipeline != VK_NULL_HANDLE)
            {
                const ComputePipeline* pPipeline = NonDispatchable<VkPipeline, ComputePipeline>::ObjectFromHandle(pipeline);

                pPipeline->BindToCmdBuffer(this);

                m_state.allGpuState.pComputePipeline = pPipeline;
                pNewLayout = pPipeline->GetLayout();
            }
            else
            {
                ComputePipeline::BindNullPipeline(this);
                m_state.allGpuState.pComputePipeline = nullptr;
            }
        }
        break;

    case VK_PIPELINE_BIND_POINT_GRAPHICS:
        {
            bindPoint = static_cast<uint32_t>(Pal::PipelineBindPoint::Graphics);

            if (pipeline != VK_NULL_HANDLE)
            {
                const GraphicsPipeline* pPipeline = NonDispatchable<VkPipeline, GraphicsPipeline>::ObjectFromHandle(pipeline);

                if (pPipeline != m_state.allGpuState.pGraphicsPipeline)
                {
                    pPipeline->BindToCmdBuffer(this, &m_state, &m_stencilCombiner);

                    m_vbMgr.GraphicsPipelineChanged(this, pPipeline);

                    m_state.allGpuState.pGraphicsPipeline = pPipeline;
                    pNewLayout = pPipeline->GetLayout();
                }
            }
            else
            {
                GraphicsPipeline::BindNullPipeline(this);
                m_state.allGpuState.pGraphicsPipeline = nullptr;
            }
        }
        break;

    default:
        {
            VK_NEVER_CALLED();
        }
        break;
    }

    if (pNewLayout != nullptr)
    {
        RebindCompatibleUserData(bindPoint, pNewLayout);
    }

    DbgBarrierPostCmd(DbgBarrierBindPipeline);
}

// =====================================================================================================================
// Called during vkCmdBindPipeline when the new pipeline's layout might be different from the previously bound layout.
// This function will compare the compatibility of those layouts and reprogram any user data to maintain previously-
// written pipeline resources to make them available in the correct locations of the new pipeline layout.
// compatible with the new layout remain correctly bound.
void CmdBuffer::RebindCompatibleUserData(
    uint32_t               bindPoint,
    const PipelineLayout*  pLayout)
{
    VK_ASSERT(pLayout != nullptr);

    Pal::PipelineBindPoint palBindPoint = static_cast<Pal::PipelineBindPoint>(bindPoint);
    PipelineBindState* pBindState = &m_state.allGpuState.pipelineState[bindPoint];

    if (pLayout == pBindState->pLayout)
    {
        VK_ASSERT(memcmp(&pLayout->GetInfo().userDataLayout, &pBindState->userDataLayout,
            sizeof(pBindState->userDataLayout)) == 0);
    }
    else
    {
        const auto& userDataLayout = pLayout->GetInfo().userDataLayout;

        // Rebind descriptor set bindings if necessary
        if (userDataLayout.setBindingRegBase  != pBindState->userDataLayout.setBindingRegBase ||
            userDataLayout.setBindingRegCount != pBindState->userDataLayout.setBindingRegCount)
        {
            const uint32_t count = Util::Min(userDataLayout.setBindingRegCount, pBindState->boundSetCount);

            if (count > 0)
            {
                uint32_t deviceIdx = 0;
                do
                {
                    PalCmdBuffer(deviceIdx)->CmdSetUserData(palBindPoint,
                                                            userDataLayout.setBindingRegBase,
                                                            count,
                                                            m_state.perGpuState[deviceIdx].setBindingData[bindPoint]);

                    deviceIdx++;
                } while (deviceIdx < m_pDevice->NumPalDevices());
            }
        }

        // Rebind push constants if necessary
        if (userDataLayout.pushConstRegBase  != pBindState->userDataLayout.pushConstRegBase ||
            userDataLayout.pushConstRegCount != pBindState->userDataLayout.pushConstRegCount)
        {
            const uint32_t count = Util::Min(userDataLayout.pushConstRegCount, pBindState->pushedConstCount);

            if (count > 0)
            {
                const uint32_t perDeviceStride = 0;
                // perDeviceStride is zero here because push constant data is replicated for all devices.
                // Note: There might be interesting use cases where don't want to clone this data.

                PalCmdBufferSetUserData(
                    palBindPoint,
                    userDataLayout.pushConstRegBase,
                    count,
                    perDeviceStride,
                    pBindState->pushConstData);
            }
        }

        // Cache the new user data layout information
        pBindState->userDataLayout = userDataLayout;
        pBindState->pLayout        = pLayout;
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

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::ICmdBuffer* pPalNestedCmdBuffer = pInteralCmdBuf->PalCmdBuffer(deviceIdx);
            PalCmdBuffer(deviceIdx)->CmdExecuteNestedCmdBuffers(1, &pPalNestedCmdBuffer);
        }
    }

    DbgBarrierPostCmd(DbgBarrierExecuteCommands);
}

// =====================================================================================================================
// Destroy a command buffer object
VkResult CmdBuffer::Destroy(void)
{
    Instance* const pInstance = m_pDevice->VkInstance();

    if (m_pSqttState != nullptr)
    {
        Util::Destructor(m_pSqttState);

        pInstance->FreeMem(m_pSqttState);
    }

    // Free per-attachment render pass instance memory
    if (m_renderPassInstance.pAttachments != nullptr)
    {
        m_pDevice->VkInstance()->FreeMem(m_renderPassInstance.pAttachments);
    }

    // Unregister this command buffer from the pool
    m_pCmdPool->UnregisterCmdBuffer(this);

    PalCmdBufferDestroy();

    // Release the GPU event manager back to the command pool
    if (m_pGpuEventMgr != nullptr)
    {
        m_pCmdPool->ReleaseGpuEventMgr(m_pGpuEventMgr);
        m_pGpuEventMgr = nullptr;
    }

    if (m_pStackAllocator != nullptr)
    {
        pInstance->StackMgr()->ReleaseAllocator(m_pStackAllocator);
        m_pStackAllocator = nullptr;
    }

    Util::Destructor(this);

    pInstance->FreeMem(ApiCmdBuffer::FromObject(this));

    return VK_SUCCESS;
}

// =====================================================================================================================
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
        const Pal::PipelineBindPoint bindPoint = (pipelineBindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) ?
            Pal::PipelineBindPoint::Graphics :
            Pal::PipelineBindPoint::Compute;

        // Get the current binding state in the command buffer
        PipelineBindState* pBindState = &m_state.allGpuState.pipelineState[static_cast<uint32_t>(bindPoint)];

        const PipelineLayout* pLayout = PipelineLayout::ObjectFromHandle(layout);

        // Get user data register information from the given pipeline layout
        const PipelineLayout::Info& layoutInfo = pLayout->GetInfo();

        // Update descriptor set binding data shadow.
        VK_ASSERT((firstSet + setCount) <= layoutInfo.setCount);

        const uint32_t numPalDevices = m_pDevice->NumPalDevices();

        for (uint32_t i = 0; i < setCount; ++i)
        {
            // Compute set binding point index
            const uint32_t setBindIdx = firstSet + i;

            // User data information for this set
            const PipelineLayout::SetUserDataLayout& setLayoutInfo = layoutInfo.setUserData[setBindIdx];

            // If this descriptor set has any dynamic descriptor data then write them into the shadow.
            if (setLayoutInfo.dynDescDataRegCount > 0)
            {
                // NOTE: We currently have to supply patched SRDs directly in used data registers. If we'll have proper
                // support for dynamic descriptors in SC then we'll only need to write the dynamic offsets directly.
                DescriptorSet::PatchedDynamicDataFromHandle(
                    pDescriptorSets[i],
                    &(m_state.perGpuState[DefaultDeviceIndex].
                        setBindingData[static_cast<uint32_t>(bindPoint)][setLayoutInfo.dynDescDataRegOffset]),
                    pDynamicOffsets,
                    setLayoutInfo.dynDescCount);

                // Skip over the already consumed dynamic offsets.
                pDynamicOffsets += setLayoutInfo.dynDescCount;
            }

            // If this descriptor set needs a set pointer, then write it to the shadow.
            if (setLayoutInfo.setPtrRegOffset != PipelineLayout::InvalidReg)
            {
                uint32_t deviceIdx = 0;
                do
                {
                    DescriptorSet::UserDataPtrValueFromHandle(
                        pDescriptorSets[i],
                        deviceIdx,
                        &(m_state.perGpuState[deviceIdx].
                            setBindingData[static_cast<uint32_t>(bindPoint)][setLayoutInfo.setPtrRegOffset]));

                    deviceIdx++;
                } while (deviceIdx < numPalDevices);
            }
        }

        // Figure out the total range of user data registers written by this sequence of descriptor set binds
        const PipelineLayout::SetUserDataLayout& firstSetLayout = layoutInfo.setUserData[firstSet];
        const PipelineLayout::SetUserDataLayout& lastSetLayout = layoutInfo.setUserData[firstSet + setCount - 1];

        const uint32_t rangeOffsetBegin = firstSetLayout.firstRegOffset;
        const uint32_t rangeOffsetEnd = lastSetLayout.firstRegOffset + lastSetLayout.totalRegCount;

        // Update the high watermark of number of user data entries written for currently bound descriptor sets and
        // their dynamic offsets in the current command buffer state.
        pBindState->boundSetCount = Util::Max(pBindState->boundSetCount, rangeOffsetEnd);

        // Descriptor set with zero resource binding is allowed in spec, so we need to check this and only proceed when
        // there are at least 1 user data to update.
        const uint32_t rangeRegCount = rangeOffsetEnd - rangeOffsetBegin;

        if (rangeRegCount > 0)
        {
            // Program the user data register only if the current user data layout base matches that of the given
            // layout.  Otherwise, what's happening is that the application is binding descriptor sets for a future
            // pipeline layout (e.g. at the top of the command buffer) and this register write will be redundant.  A
            // future vkCmdBindPipeline will reprogram the user data register.
            if (pBindState->userDataLayout.setBindingRegBase == layoutInfo.userDataLayout.setBindingRegBase)
            {
                uint32_t deviceIdx = 0;
                do
                {
                    PalCmdBuffer(deviceIdx)->CmdSetUserData(
                        bindPoint,
                        pBindState->userDataLayout.setBindingRegBase + rangeOffsetBegin,
                        rangeRegCount,
                        &(m_state.perGpuState[deviceIdx].
                            setBindingData[static_cast<uint32_t>(bindPoint)][rangeOffsetBegin]));

                    deviceIdx++;
                }
                while (deviceIdx < numPalDevices);
            }
        }
    }

    DbgBarrierPostCmd(DbgBarrierBindSetsPushConstants);
}

// =====================================================================================================================
void CmdBuffer::BindIndexBuffer(
    VkBuffer     buffer,
    VkDeviceSize offset,
    VkIndexType  indexType)
{
    DbgBarrierPreCmd(DbgBarrierBindIndexVertexBuffer);

    const Pal::IndexType palIndexType = VkToPalIndexType(indexType);
    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    if (pBuffer != NULL)
    {
        PalCmdBindIndexData(pBuffer, offset, palIndexType);
    }
    else
    {
        PalCmdUnbindIndexData(palIndexType);
    }

    DbgBarrierPostCmd(DbgBarrierBindIndexVertexBuffer);
}

// =====================================================================================================================
// Implementation of vkCmdBindVertexBuffers
void CmdBuffer::BindVertexBuffers(
    uint32_t            firstBinding,
    uint32_t            bindingCount,
    const VkBuffer*     pBuffers,
    const VkDeviceSize* pOffsets)
{
    DbgBarrierPreCmd(DbgBarrierBindIndexVertexBuffer);

    m_vbMgr.BindVertexBuffers(this, firstBinding, bindingCount, pBuffers, pOffsets);

    DbgBarrierPostCmd(DbgBarrierBindIndexVertexBuffer);
}

// =====================================================================================================================
void CmdBuffer::Draw(
    uint32_t firstVertex,
    uint32_t vertexCount,
    uint32_t firstInstance,
    uint32_t instanceCount)
{
    DbgBarrierPreCmd(DbgBarrierDrawNonIndexed);

    PalCmdDraw(firstVertex,
        vertexCount,
        firstInstance,
        instanceCount);

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

    PalCmdDrawIndexed(firstIndex,
                      indexCount,
                      vertexOffset,
                      firstInstance,
                      instanceCount);

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

    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    if ((stride + offset) <= pBuffer->PalMemory(DefaultDeviceIndex)->Desc().size)
    {
        const Pal::gpusize paramOffset = pBuffer->MemOffset() + offset;
        Pal::gpusize countVirtAddr = 0;

        utils::IterateMask deviceGroup(m_palDeviceMask);

        while (deviceGroup.Iterate())
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
    }

    DbgBarrierPostCmd((indexed ? DbgBarrierDrawIndexed : DbgBarrierDrawNonIndexed) | DbgBarrierDrawIndirect);
}

// =====================================================================================================================
void CmdBuffer::Dispatch(
    uint32_t x,
    uint32_t y,
    uint32_t z)
{
    DbgBarrierPreCmd(DbgBarrierDispatch);

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

    PalCmdDispatchOffset(base_x, base_y, base_z, dim_x, dim_y, dim_z);

    DbgBarrierPostCmd(DbgBarrierDispatch);
}

// =====================================================================================================================
void CmdBuffer::DispatchIndirect(
    VkBuffer     buffer,
    VkDeviceSize offset)
{
    DbgBarrierPreCmd(DbgBarrierDispatchIndirect);

    Buffer* pBuffer = Buffer::ObjectFromHandle(buffer);

    PalCmdDispatchIndirect(pBuffer, offset);

    DbgBarrierPostCmd(DbgBarrierDispatchIndirect);
}

// =====================================================================================================================
void CmdBuffer::CopyBuffer(
    VkBuffer                                    srcBuffer,
    VkBuffer                                    destBuffer,
    uint32_t                                    regionCount,
    const VkBufferCopy*                         pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer);

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

    DbgBarrierPostCmd(DbgBarrierCopyBuffer);
}

// =====================================================================================================================
void CmdBuffer::CopyImage(
    VkImage            srcImage,
    VkImageLayout      srcImageLayout,
    VkImage            destImage,
    VkImageLayout      destImageLayout,
    uint32_t           regionCount,
    const VkImageCopy* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyImage);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions)), MaxPalAspectsPerMask);
    auto       regionBatch = Util::Min(regionCount * MaxPalAspectsPerMask, maxRegions);

    Pal::ImageCopyRegion* pPalRegions =
        virtStackFrame.AllocArray<Pal::ImageCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage    = Image::ObjectFromHandle(srcImage);
        const Image* const pDstImage    = Image::ObjectFromHandle(destImage);

        const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat());
        const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat());

        const Pal::ImageLayout palSrcImgLayout = pSrcImage->GetTransferLayout(srcImageLayout, this);
        const Pal::ImageLayout palDstImgLayout = pDstImage->GetTransferLayout(destImageLayout, this);

        for (uint32_t regionIdx = 0; regionIdx < regionCount;)
        {
            uint32_t palRegionCount = 0;

            while ((regionIdx < regionCount) &&
                   (palRegionCount <= (regionBatch - MaxPalAspectsPerMask)))
            {
                VkToPalImageCopyRegion(pRegions[regionIdx], srcFormat.format, dstFormat.format,
                    pPalRegions, palRegionCount);

                ++regionIdx;
            }

            PalCmdCopyImage(pSrcImage, palSrcImgLayout, pDstImage, palDstImgLayout, palRegionCount, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    DbgBarrierPostCmd(DbgBarrierCopyImage);
}

// =====================================================================================================================
void CmdBuffer::BlitImage(
    VkImage            srcImage,
    VkImageLayout      srcImageLayout,
    VkImage            destImage,
    VkImageLayout      destImageLayout,
    uint32_t           regionCount,
    const VkImageBlit* pRegions,
    VkFilter           filter)
{
    DbgBarrierPreCmd(DbgBarrierCopyImage);

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

        const Pal::SwizzledFormat srcFormat = VkToPalFormat(pSrcImage->GetFormat());
        const Pal::SwizzledFormat dstFormat = VkToPalFormat(pDstImage->GetFormat());

        Pal::ScaledCopyInfo palCopyInfo = {};

        palCopyInfo.srcImageLayout = pSrcImage->GetTransferLayout(srcImageLayout, this);
        palCopyInfo.dstImageLayout = pDstImage->GetTransferLayout(destImageLayout, this);

        // Maps blit filters to their PAL equivalent
        palCopyInfo.filter   = VkToPalTexFilter(VK_FALSE, filter, filter, VK_SAMPLER_MIPMAP_MODE_NEAREST);
        palCopyInfo.rotation = Pal::ImageRotation::Ccw0;

        palCopyInfo.pRegions = pPalRegions;

        for (uint32_t regionIdx = 0; regionIdx < regionCount;)
        {
            palCopyInfo.regionCount = 0;

            while ((regionIdx < regionCount) &&
                   (palCopyInfo.regionCount <= (regionBatch - MaxPalAspectsPerMask)))
            {
                VkToPalImageScaledCopyRegion(pRegions[regionIdx], srcFormat.format, dstFormat.format,
                    pPalRegions, palCopyInfo.regionCount);

                ++regionIdx;
            }

            // This will do a scaled blit
            PalCmdScaledCopyImage(pSrcImage, pDstImage, palCopyInfo);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    DbgBarrierPostCmd(DbgBarrierCopyImage);
}

// PAL version 391.1 adds support for mis-aligned buffer-image/image-buffer copies
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 391) || \
    ((PAL_CLIENT_INTERFACE_MAJOR_VERSION == 391) && (PAL_CLIENT_INTERFACE_MINOR_VERSION < 1))
// =====================================================================================================================
// Align memory to image copy region
void CmdBuffer::AlignMemoryImageCopyRegion(
    const Pal::IImage*          pImage,
    Pal::MemoryImageCopyRegion* pRegion) const
{
    VK_ASSERT(m_palQueueType == Pal::QueueTypeDma);

    // Copying from a buffer to image on the DMA Queue requires the width/height to be byte aligned.
    // According to the OSS spec X and Rect X should be aligned to DW (multiple of four for 8bpp,
    // multiple of 2 for 16bpp)

    auto bytesPerPixel = Pal::Formats::BytesPerPixel(pImage->GetImageCreateInfo().swizzledFormat.format);

    const uint32_t copySizeDwordsWidth =
        Util::NumBytesToNumDwords(pRegion->imageExtent.width * bytesPerPixel);
    const uint32_t copySizeBytesWidth  = copySizeDwordsWidth * sizeof(uint32_t);
    const uint32_t copySizePixelsWidth = copySizeBytesWidth  / bytesPerPixel;
    pRegion->imageExtent.width = copySizePixelsWidth;

    const uint32_t copySizeDwordsHeight =
        Util::NumBytesToNumDwords(pRegion->imageExtent.height * bytesPerPixel);
    const uint32_t copySizeBytesHeight  = copySizeDwordsHeight * sizeof(uint32_t);
    const uint32_t copySizePixelsHeight = copySizeBytesHeight  / bytesPerPixel;
    pRegion->imageExtent.height = copySizePixelsHeight;
}
#endif

// =====================================================================================================================
// Copies from a buffer of linear data to a region of an image (vkCopyBufferToImage)
void CmdBuffer::CopyBufferToImage(
    VkBuffer                  srcBuffer,
    VkImage                   destImage,
    VkImageLayout             destImageLayout,
    uint32_t                  regionCount,
    const VkBufferImageCopy*  pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);

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

        const Pal::ImageLayout layout = pDstImage->GetTransferLayout(destImageLayout, this);

        for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
        {
            regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

            for (uint32_t i = 0; i < regionBatch; ++i)
            {
                // For image-buffer copies we have to override the format for depth-only and stencil-only copies
                Pal::SwizzledFormat dstFormat = VkToPalFormat(Formats::GetAspectFormat(
                    pDstImage->GetFormat(), pRegions[regionIdx + i].imageSubresource.aspectMask));

                pPalRegions[i] = VkToPalMemoryImageCopyRegion(pRegions[regionIdx + i], dstFormat.format, srcMemOffset);

                // PAL version 391.1 adds support for mis-aligned buffer-image/image-buffer copies
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 391) || \
    ((PAL_CLIENT_INTERFACE_MAJOR_VERSION == 391) && (PAL_CLIENT_INTERFACE_MINOR_VERSION < 1))
                if (!GpuUtil::ValidateMemoryImageRegion(
                    m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties(),
                    m_palEngineType,
                    *pDstImage->PalImage(),
                    *pSrcBuffer->PalMemory(),
                    pPalRegions[i]))
                {
                     AlignMemoryImageCopyRegion(pDstImage->PalImage(), &pPalRegions[i]);
                }
#endif
            }

            PalCmdCopyMemoryToImage(pSrcBuffer, pDstImage, layout, regionBatch, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    DbgBarrierPostCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);
}

// =====================================================================================================================
// Copies and detiles a region of an image to a buffer (vkCopyImageToBuffer)
void CmdBuffer::CopyImageToBuffer(
    VkImage                  srcImage,
    VkImageLayout            srcImageLayout,
    VkBuffer                 destBuffer,
    uint32_t                 regionCount,
    const VkBufferImageCopy* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierCopyBuffer | DbgBarrierCopyImage);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRegions  = EstimateMaxObjectsOnVirtualStack(sizeof(*pRegions));
    auto       regionBatch = Util::Min(regionCount, maxRegions);

    // Allocate space to store memory image copy regions
    Pal::MemoryImageCopyRegion* pPalRegions = virtStackFrame.AllocArray<Pal::MemoryImageCopyRegion>(regionBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage      = Image::ObjectFromHandle(srcImage);
        Buffer* pDstBuffer                = Buffer::ObjectFromHandle(destBuffer);
        const Pal::IImage& palImage       = *pSrcImage->PalImage(DefaultDeviceIndex);
        Pal::IGpuMemory* const pDstMemory = pDstBuffer->PalMemory(DefaultDeviceIndex);
        const Pal::gpusize dstMemOffset   = pDstBuffer->MemOffset();

        const Pal::ImageLayout layout = pSrcImage->GetTransferLayout(srcImageLayout, this);

        for (uint32_t regionIdx = 0; regionIdx < regionCount; regionIdx += regionBatch)
        {
            regionBatch = Util::Min(regionCount - regionIdx, maxRegions);

            for (uint32_t i = 0; i < regionBatch; ++i)
            {
                // For image-buffer copies we have to override the format for depth-only and stencil-only copies
                Pal::SwizzledFormat srcFormat = VkToPalFormat(Formats::GetAspectFormat(pSrcImage->GetFormat(),
                    pRegions[regionIdx + i].imageSubresource.aspectMask));

                pPalRegions[i] = VkToPalMemoryImageCopyRegion(pRegions[regionIdx + i], srcFormat.format, dstMemOffset);
            }

            PalCmdCopyImageToMemory(pSrcImage, pDstBuffer, layout, regionBatch, pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

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

    Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    PalCmdUpdateBuffer(pDestBuffer, pDestBuffer->MemOffset() + destOffset, dataSize, pData);

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

    Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    if (fillSize == VK_WHOLE_SIZE)
    {
        fillSize = Util::RoundDownToMultiple(pDestBuffer->GetSize() - destOffset, static_cast<VkDeviceSize>(sizeof(data) ) );
    }

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        PalCmdFillBuffer(pDestBuffer, pDestBuffer->MemOffset() + destOffset, fillSize, data);
    }

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
    const Image* pImage = Image::ObjectFromHandle(image);

    const Pal::SwizzledFormat palFormat = VkToPalFormat(pImage->GetFormat());

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
        const Pal::ImageLayout layout = pImage->GetTransferLayout(imageLayout, this);

        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount;)
        {
            uint32_t palRangeCount = 0;

            while ((rangeIdx < rangeCount) &&
                   (palRangeCount <= (rangeBatch - MaxPalColorAspectsPerMask)))
            {
                // Only color aspect is allowed here
                VK_ASSERT(pRanges[rangeIdx].aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);

                VkToPalSubresRange(palFormat.format,
                                   pRanges[rangeIdx],
                                   pImage->GetMipLevels(),
                                   pImage->GetArraySize(),
                                   pPalRanges,
                                   palRangeCount);

                ++rangeIdx;
            }

            PalCmdClearColorImage(
                *pImage,
                layout,
                VkToPalClearColor(pColor, palFormat.format),
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
}

// =====================================================================================================================
void CmdBuffer::PreBltBindMsaaState(
    const Image& image)
{
    if (GetPalQueueType() == Pal::QueueTypeUniversal)
    {
        VK_ASSERT(m_state.allGpuState.pBltMsaaStates == nullptr);

        const Pal::IMsaaState* const * pBltMsaa = nullptr;
        const Pal::ImageCreateInfo& imgInfo = image.PalImage(DefaultDeviceIndex)->GetImageCreateInfo();

        if (imgInfo.samples > 1)
        {
            pBltMsaa = m_pDevice->GetBltMsaaState(imgInfo.samples);
        }

        PalCmdBindMsaaStates(pBltMsaa);

        m_state.allGpuState.pBltMsaaStates = pBltMsaa;
    }
}

// =====================================================================================================================
void CmdBuffer::PostBltRestoreMsaaState()
{
    if (GetPalQueueType() == Pal::QueueTypeUniversal)
    {
        if ((m_state.allGpuState.pBltMsaaStates != nullptr) &&
            (m_state.allGpuState.pGraphicsPipeline != nullptr))
        {
            PalCmdBindMsaaStates(m_state.allGpuState.pGraphicsPipeline->GetMsaaStates());
        }

        m_state.allGpuState.pBltMsaaStates = nullptr;
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
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRanges  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRanges)), MaxPalDepthAspectsPerMask);
    auto       rangeBatch = Util::Min(rangeCount * MaxPalDepthAspectsPerMask, maxRanges);

    // Allocate space to store image subresource ranges (we need a separate region per PAL aspect)
    Pal::SubresRange* pPalRanges = virtStackFrame.AllocArray<Pal::SubresRange>(rangeBatch);

    if (pPalRanges != nullptr)
    {
        const Image* pImage           = Image::ObjectFromHandle(image);
        const Pal::ImageLayout layout = pImage->GetTransferLayout(imageLayout, this);

        for (uint32_t rangeIdx = 0; rangeIdx < rangeCount;)
        {
            uint32_t palRangeCount = 0;

            while ((rangeIdx < rangeCount) &&
                   (palRangeCount <= (rangeBatch - MaxPalDepthAspectsPerMask)))
            {
                // Only depth or stencil aspect is allowed here
                VK_ASSERT((pRanges[rangeIdx].aspectMask & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0);

                VkToPalSubresRange(VkToPalFormat(pImage->GetFormat()).format,
                                   pRanges[rangeIdx],
                                   pImage->GetMipLevels(),
                                   pImage->GetArraySize(),
                                   pPalRanges,
                                   palRangeCount);

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
}

// =====================================================================================================================
// Clears a set of attachments in the current subpass
void CmdBuffer::ClearAttachments(
    uint32_t                 attachmentCount,
    const VkClearAttachment* pAttachments,
    uint32_t                 rectCount,
    const VkClearRect*       pRects)
{
    if ((m_is2ndLvl == false) && (m_state.allGpuState.pFramebuffer != nullptr))
    {
        ClearImageAttachments(attachmentCount, pAttachments, rectCount, pRects);
    }
    else
    {
        ClearBoundAttachments(attachmentCount, pAttachments, rectCount, pRects);
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
    const RenderPass* pRenderPass = m_state.allGpuState.pRenderPass;
    const uint32_t subpass        = m_renderPassInstance.subpass;

    Util::Vector<Pal::ClearBoundTargetRegion, 8, VirtualStackFrame> clearRegions { &virtStackFrame };
    Util::Vector<Pal::BoundColorTarget,       8, VirtualStackFrame> colorTargets { &virtStackFrame };

    const auto maxRects   = EstimateMaxObjectsOnVirtualStack(sizeof(*pRects));
    auto       rectBatch  = Util::Min(rectCount, maxRects);
    const auto palResult1 = clearRegions.Reserve(rectBatch);
    const auto palResult2 = colorTargets.Reserve(attachmentCount);

    if ((palResult1 != Pal::Result::Success) ||
        (palResult2 != Pal::Result::Success))
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;

        return;
    }

    for (uint32_t idx = 0; idx < attachmentCount; ++idx)
    {
        const VkClearAttachment& clearInfo = pAttachments[idx];

        // Detect if color clear or depth clear
        if ((clearInfo.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0)
        {
            // Get the corresponding color reference in the current subpass
            const VkAttachmentReference& colorRef = pRenderPass->GetSubpassColorReference(
                subpass, clearInfo.colorAttachment);

            // Clear only if the attachment reference is active
            if (colorRef.attachment != VK_ATTACHMENT_UNUSED)
            {
                // Fill in bound target information for this target, but don't clear yet
                const uint32_t tgtIdx = clearInfo.colorAttachment;

                Pal::BoundColorTarget target { };
                target.targetIndex    = tgtIdx;
                target.swizzledFormat = VkToPalFormat(pRenderPass->GetColorAttachmentFormat(subpass, tgtIdx));
                target.samples        = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
                target.fragments      = pRenderPass->GetColorAttachmentSamples(subpass, tgtIdx);
                target.clearValue     = VkToPalClearColor(&clearInfo.clearValue.color, target.swizzledFormat.format);

                colorTargets.PushBack(target);
            }
        }
        else // Depth-stencil clear
        {
            // Get the corresponding color reference in the current subpass
            const VkAttachmentReference& depthStencilRef = pRenderPass->GetSubpassDepthStencilReference(subpass);

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

                    CreateClearRegions(
                        rectBatch, pRects + rectIdx,
                        *pRenderPass, subpass,
                        &clearRegions);

                    // Clear the bound depth stencil target immediately
                    PalCmdBuffer(DefaultDeviceIndex)->CmdClearBoundDepthStencilTargets(
                        VkToPalClearDepth(clearInfo.clearValue.depthStencil.depth),
                        clearInfo.clearValue.depthStencil.stencil,
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

            CreateClearRegions(
                rectBatch, pRects + rectIdx,
                *pRenderPass, subpass,
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

// =====================================================================================================================
void CmdBuffer::PalCmdClearColorImage(
    const Image&            image,
    Pal::ImageLayout        imageLayout,
    const Pal::ClearColor&  color,
    uint32_t                rangeCount,
    const Pal::SubresRange* pRanges,
    uint32_t                boxCount,
    const Pal::Box*         pBoxes,
    uint32_t                flags)
{
    DbgBarrierPreCmd(DbgBarrierClearColor);

    PreBltBindMsaaState(image);

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdClearColorImage(
            *image.PalImage(deviceIdx),
            imageLayout,
            color,
            rangeCount,
            pRanges,
            boxCount,
            pBoxes,
            flags);
    }

    PostBltRestoreMsaaState();

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

    PreBltBindMsaaState(image);

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdClearDepthStencil(
            *image.PalImage(deviceIdx),
            depthLayout,
            stencilLayout,
            depth,
            stencil,
            rangeCount,
            pRanges,
            rectCount,
            pRects,
            flags);
    }

    PostBltRestoreMsaaState();

    DbgBarrierPostCmd(DbgBarrierClearDepth);
}

// =====================================================================================================================
template <typename EventContainer_T>
void CmdBuffer::PalCmdResetEvent(
    EventContainer_T*       pEvent,
    Pal::HwPipePoint        resetPoint)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdResetEvent(*pEvent->PalEvent(deviceIdx), resetPoint);
    }
}

// =====================================================================================================================
// Instantiate the template function
template void CmdBuffer::PalCmdResetEvent<GpuEvents>(
    GpuEvents*              pEvent,
    Pal::HwPipePoint        resetPoint);

// =====================================================================================================================
template <typename EventContainer_T>
void CmdBuffer::PalCmdSetEvent(
    EventContainer_T*       pEvent,
    Pal::HwPipePoint        setPoint)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdSetEvent(*pEvent->PalEvent(deviceIdx), setPoint);
    }
}

// =====================================================================================================================
// Instantiate the template function
template void CmdBuffer::PalCmdSetEvent<GpuEvents>(
    GpuEvents*              pEvent,
    Pal::HwPipePoint        resetPoint);

// =====================================================================================================================
template<bool regionPerDevice>
void CmdBuffer::PalCmdResolveImage(
    const Image&                   srcImage,
    Pal::ImageLayout               srcImageLayout,
    const Image&                   dstImage,
    Pal::ImageLayout               dstImageLayout,
    uint32_t                       regionCount,
    const Pal::ImageResolveRegion* pRegions)
{
    DbgBarrierPreCmd(DbgBarrierResolve);

    PreBltBindMsaaState(srcImage);

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdResolveImage(
                    *srcImage.PalImage(deviceIdx),
                    srcImageLayout,
                    *dstImage.PalImage(deviceIdx),
                    dstImageLayout,
                    Pal::ResolveMode::Average,
                    regionCount,
                    pRegions + (regionPerDevice ? (regionCount * deviceIdx) : 0) );
    }

    PostBltRestoreMsaaState();

    DbgBarrierPostCmd(DbgBarrierResolve);
}

// =====================================================================================================================
// Instantiate the template function
template void CmdBuffer::PalCmdResolveImage<true>(
    const Image&                   srcImage,
    Pal::ImageLayout               srcImageLayout,
    const Image&                   dstImage,
    Pal::ImageLayout               dstImageLayout,
    uint32_t                       regionCount,
    const Pal::ImageResolveRegion* pRegions);

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
    const RenderPass* pRenderPass = m_state.allGpuState.pRenderPass;
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
            const VkAttachmentReference& colorRef = pRenderPass->GetSubpassColorReference(subpass, targetIdx);

            // Get the referenced attachment index in the framebuffer
            const uint32_t attachmentIdx = colorRef.attachment;

            // Clear only if the referenced attachment index is active
            if (attachmentIdx != VK_ATTACHMENT_UNUSED)
            {
                // Get the matching framebuffer attachment
                const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

                // Get the layout that this color attachment is currently in within the render pass
                const Pal::ImageLayout targetLayout = RPGetAttachmentLayout(attachmentIdx, Pal::ImageAspect::Color);

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
                        rectBatch = Util::Min(rectCount - rectIdx, maxRects);

                        CreateClearRegions(
                            rectCount, pRects + rectIdx,
                            *pRenderPass, subpass,
                            &clearBoxes);

                        CreateClearSubresRanges(
                            attachment, clearInfo,
                            rectCount, pRects + rectIdx,
                            *pRenderPass, subpass,
                            &clearSubresRanges);

                        PalCmdClearColorImage(
                            *attachment.pImage,
                            targetLayout,
                            VkToPalClearColor(&clearInfo.clearValue.color, attachment.viewFormat.format),
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
            const VkAttachmentReference& depthStencilRef = pRenderPass->GetSubpassDepthStencilReference(subpass);

            // Get the referenced attachment index in the framebuffer
            const uint32_t attachmentIdx = depthStencilRef.attachment;

            // Clear only if the referenced attachment index is active
            if (attachmentIdx != VK_ATTACHMENT_UNUSED)
            {
                // Get the matching framebuffer attachment
                const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

                // Get the layout(s) that this attachment is currently in within the render pass
                const Pal::ImageLayout depthLayout   = RPGetAttachmentLayout(attachmentIdx, Pal::ImageAspect::Depth);
                const Pal::ImageLayout stencilLayout = RPGetAttachmentLayout(attachmentIdx, Pal::ImageAspect::Stencil);

                Util::Vector<Pal::Rect,        8, VirtualStackFrame> clearRects        { &virtStackFrame };
                Util::Vector<Pal::SubresRange, 8, VirtualStackFrame> clearSubresRanges { &virtStackFrame };

                auto       rectBatch  = Util::Min(rectCount, maxRects);
                const auto palResult1 = clearRects.Reserve(rectBatch);
                const auto palResult2 = clearSubresRanges.Reserve(rectBatch);

                if ((palResult1 == Pal::Result::Success) &&
                    (palResult2 == Pal::Result::Success))
                {
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
void CmdBuffer::ResolveImage(
    VkImage               srcImage,
    VkImageLayout         srcImageLayout,
    VkImage               destImage,
    VkImageLayout         destImageLayout,
    uint32_t              rectCount,
    const VkImageResolve* pRects)
{
    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    const auto maxRects  = Util::Max(EstimateMaxObjectsOnVirtualStack(sizeof(*pRects)), MaxPalAspectsPerMask);
    auto       rectBatch = Util::Min(rectCount * MaxPalAspectsPerMask, maxRects);

    // Allocate space to store image resolve regions (we need a separate region per PAL aspect)
    Pal::ImageResolveRegion* pPalRegions =
        virtStackFrame.AllocArray<Pal::ImageResolveRegion>(rectBatch);

    if (pPalRegions != nullptr)
    {
        const Image* const pSrcImage              = Image::ObjectFromHandle(srcImage);
        const Image* const pDstImage              = Image::ObjectFromHandle(destImage);
        const Pal::ImageLayout palSrcImageLayout  = pSrcImage->GetTransferLayout(srcImageLayout, this);
        const Pal::ImageLayout palDestImageLayout = pDstImage->GetTransferLayout(destImageLayout, this);
        const Pal::SwizzledFormat srcFormat       = VkToPalFormat(pSrcImage->GetFormat());
        const Pal::SwizzledFormat dstFormat       = VkToPalFormat(pDstImage->GetFormat());

        for (uint32_t rectIdx = 0; rectIdx < rectCount;)
        {
            uint32_t palRegionCount = 0;

            while ((rectIdx < rectCount) &&
                   (palRegionCount <= (rectBatch - MaxPalAspectsPerMask)))
            {
                // We expect MSAA images to never have mipmaps
                VK_ASSERT(pRects[rectIdx].srcSubresource.mipLevel == 0);

                VkToPalImageResolveRegion(pRects[rectIdx], srcFormat.format, dstFormat.format, pPalRegions, palRegionCount);

                ++rectIdx;
            }

            PalCmdResolveImage<false>(
                *pSrcImage,
                palSrcImageLayout,
                *pDstImage,
                palDestImageLayout,
                palRegionCount,
                pPalRegions);
        }

        virtStackFrame.FreeArray(pPalRegions);
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
}

// =====================================================================================================================
// Implementation of vkCmdSetEvent()
void CmdBuffer::SetEvent(
    VkEvent                 event,
    VkPipelineStageFlags    stageMask)
{
    DbgBarrierPreCmd(DbgBarrierSetResetEvent);

    PalCmdSetEvent(Event::ObjectFromHandle(event), VkToPalSrcPipePoint(stageMask));

    DbgBarrierPostCmd(DbgBarrierSetResetEvent);
}

// =====================================================================================================================
void CmdBuffer::ResetEvent(
    VkEvent                 event,
    VkPipelineStageFlags    stageMask)
{
    DbgBarrierPreCmd(DbgBarrierSetResetEvent);

    Event* pEvent = Event::ObjectFromHandle(event);

    const Pal::HwPipePoint pipePoint = VkToPalSrcPipePoint(stageMask);

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdResetEvent(*pEvent->PalEvent(deviceIdx), pipePoint);
    }

    DbgBarrierPostCmd(DbgBarrierSetResetEvent);
}

// =====================================================================================================================
// Given a bitmask of VkAccessFlags, computes the representative PAL CacheCoherencyUsageFlags that will be written
// in the srcCacheMask field of a pipeline BarrierTransition.
Pal::uint32 CmdBuffer::ConvertBarrierSrcAccessFlags(
    const Device* pDevice,
    VkAccessFlags accessMask)
{
    Pal::uint32 coher = 0;

    if (accessMask & VK_ACCESS_HOST_WRITE_BIT)
    {
        coher |= Pal::CoherCpu;
    }

    if (accessMask & VK_ACCESS_SHADER_WRITE_BIT)
    {
        coher |= Pal::CoherShader;
    }

    if (accessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
    {
        coher |= Pal::CoherColorTarget | Pal::CoherClear;
    }

    if (accessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
    {
        coher |= Pal::CoherDepthStencilTarget | Pal::CoherClear;
    }

    if (accessMask & VK_ACCESS_TRANSFER_WRITE_BIT)
    {
        // Also need Pal::CoherShader here as vkCmdCopyQueryPoolResults uses a compute shader defined in the Vulkan
        // API layer when used with timestamp queries.
        coher |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherClear | Pal::CoherShader;

        if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_BUFFER_MARKER))
        {
            coher |= Pal::CoherTimestamp;
        }
    }

    if (accessMask & VK_ACCESS_MEMORY_WRITE_BIT)
    {
        coher |= Pal::CoherMemory;
    }

    // CoherQueueAtomic: Not used
    // CoherTimestamp: Timestamp write syncs are handled by the timestamp-related write/query funcs and not barriers
    // CoherCeLoad: Not used
    // CoherCeDump: Not used
    // CoherStreamOut: Not used

    return coher;
}

// =====================================================================================================================
// Given a bitmask of VkAccessFlags, computes the representative PAL CacheCoherencyUsageFlags that will be written
// in the dstCacheMask field of a pipeline BarrierTransition.
Pal::uint32 CmdBuffer::ConvertBarrierDstAccessFlags(
    const Device* pDevice,
    VkAccessFlags accessMask,
    uint32_t      combinedAccessMask)
{
    // Assure that the resource barrier provide combined src/dst accessmask
    // No need to concern that access masks are provided by separate barriers
    if (combinedAccessMask)
    {
        Pal::uint32 coher = 0;

        if (accessMask & VK_ACCESS_HOST_READ_BIT)
        {
            coher |= Pal::CoherCpu;
        }

        if (accessMask & VK_ACCESS_SHADER_READ_BIT)
        {
            coher |= Pal::CoherShader;
        }

        if (accessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
        {
            coher |= Pal::CoherColorTarget;
        }

        if (accessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
        {
            coher |= Pal::CoherDepthStencilTarget;
        }

        if (accessMask & VK_ACCESS_TRANSFER_READ_BIT)
        {
            // Also need Pal::CoherShader here as vkCmdCopyQueryPoolResults uses a compute shader defined in the Vulkan
            // API layer when used with timestamp queries.
            coher |= Pal::CoherCopy | Pal::CoherResolve | Pal::CoherShader;
        }

        if (accessMask & VK_ACCESS_MEMORY_READ_BIT)
        {
            coher |= Pal::CoherMemory;
        }

        if (accessMask & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
        {
            coher |= Pal::CoherShader;
        }

        if (accessMask & VK_ACCESS_UNIFORM_READ_BIT)
        {
            coher |= Pal::CoherShader;
        }

        if (accessMask & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)
        {
            coher |= Pal::CoherShader;
        }

        if (accessMask & VK_ACCESS_INDEX_READ_BIT)
        {
            coher |= Pal::CoherIndexData;
        }

        if (accessMask & VK_ACCESS_INDEX_READ_BIT)
        {
            coher |= Pal::CoherIndexData;
        }

        if (accessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
        {
            coher |= Pal::CoherIndirectArgs;
        }

        if (accessMask & VK_ACCESS_COMMAND_PROCESS_READ_BIT_NVX)
        {
            coher |= Pal::CoherMemory;
        }

        return coher;
    }
    else
    {
        // With the more loose memory barrier semantics introduced we practically have to always invalidate all relevant
        // caches. The complete set is limited based on the usage allowed by the resource at the caller side.
        // The current Vulkan Spec has language as:
        //
        // Memory dependency m guarantees that:
        //   Memory writes in a' are made available.
        //   Available memory writes, including those from a', are made visible to b'.
        // Since PAL's model is to use both SrcAcess and DstAccess flag to define memory access dependency, it may not
        // always to be able to take action for Vulkan a' available or previous write availablity with some type of Vulkan
        // access masks, e.g. DstAccessMask = 0. I believe this is the reason Vulkan forces a conservertive dstAccessMask.
        //
        // Vulkan needs to be aware that for ASIC that has L2 coherency for most of the operations (but not all), what we
        // do likely make all barriers with memory dependency to flush/inv L2.
        constexpr uint32_t AllInputCaches = Pal::CoherCpu |
                                            Pal::CoherShader |
                                            Pal::CoherCopy |
                                            Pal::CoherColorTarget |
                                            Pal::CoherDepthStencilTarget |
                                            Pal::CoherResolve |
                                            Pal::CoherClear |
                                            Pal::CoherIndirectArgs |
                                            Pal::CoherIndexData |
                                            Pal::CoherQueueAtomic |
                                            Pal::CoherTimestamp |
                                            Pal::CoherCeLoad |
                                            Pal::CoherCeDump |
                                            Pal::CoherStreamOut |
                                            Pal::CoherMemory;
        return AllInputCaches;
    }

}

// =====================================================================================================================
// Convert src access and dst access mask to the PAL CacheCoherencyUsageFlags that will be written
// in the srcCacheMask and dstCacheMask field of a pipeline BarrierTransition.
void CmdBuffer::ConvertBarrierCacheFlags(
    const Device*           pDevice,
    VkAccessFlags           srcAccess,
    VkAccessFlags           dstAccess,
    uint32_t                supportInputCacheMask,
    uint32_t                supportOutputCacheMask,
    uint32_t                barrierOptions,
    Pal::BarrierTransition* pResult)
{
     pResult->srcCacheMask = supportOutputCacheMask & ConvertBarrierSrcAccessFlags(pDevice, srcAccess);

     // srccachemask is 0 for all read only source access like VK_ACCESS_*_READ_BIT
     // etc. hence, only validate against all input caches if we are going from write to any other access flag.
     // The input shader caches only needs to be flushed if something was written and it hasn't been flushed.

     if ((pResult->srcCacheMask == 0) && (barrierOptions & SkipDstCacheFlush))
     {
         pResult->dstCacheMask = 0;
     }
     else
     {
         pResult->dstCacheMask = supportInputCacheMask & ConvertBarrierDstAccessFlags(
                                                            pDevice,
                                                            dstAccess,
                                                            (barrierOptions & CombinedAccessMasks));
     }
}

// =====================================================================================================================
// Helper function called from ExecuteBarriers
void CmdBuffer::FlushBarriers(
    Pal::BarrierInfo*              pBarrier,
    Pal::BarrierTransition* const  pTransitions,
    const Image**                  pTransitionImages,
    uint32_t                       mainTransitionCount,
    uint32_t                       postTransitionStartIdx,
    uint32_t                       postTransitionCount)
{
    pBarrier->transitionCount = mainTransitionCount;
    pBarrier->pTransitions    = pTransitions;

    PalCmdBarrier(pBarrier, pTransitions, pTransitionImages);

    if (postTransitionCount > 0)
    {
        Pal::BarrierInfo postBarrier = {};

        postBarrier.reason             = pBarrier->reason;
        postBarrier.waitPoint          = pBarrier->waitPoint;
        postBarrier.pipePointWaitCount = pBarrier->pipePointWaitCount;
        postBarrier.pPipePoints        = pBarrier->pPipePoints;
        postBarrier.transitionCount    = postTransitionCount;
        postBarrier.pTransitions       = pTransitions + postTransitionStartIdx;

        PalCmdBarrier(&postBarrier,
                      pTransitions      + postTransitionStartIdx,
                      pTransitionImages + postTransitionStartIdx);
    }
}

// =====================================================================================================================
// ExecuteBarriers  Called by vkCmdWaitEvents() and vkCmdPipelineBarrier().
void CmdBuffer::ExecuteBarriers(
    VirtualStackFrame&           virtStackFrame,
    uint32_t                     memBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers,
    Pal::BarrierInfo*            pBarrier)
{
    if ((memBarrierCount + bufferMemoryBarrierCount + imageMemoryBarrierCount + pBarrier->gpuEventWaitCount) == 0)
    {
        return;
    }

    constexpr uint32_t MaxTransitionCount = 512;
    constexpr uint32_t MaxLocationCount = 128;

    Pal::BarrierTransition* pTransitions     = virtStackFrame.AllocArray<Pal::BarrierTransition>(MaxTransitionCount);
    Pal::BarrierTransition* pPostTransitions = pTransitions + (MaxTransitionCount - 1);
    Pal::BarrierTransition* pNextMain        = pTransitions;
    Pal::BarrierTransition* pNextPost        = pPostTransitions;

    if (pTransitions == nullptr)
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;

        return;
    }

    const Image** pTransitionImages = (m_pDevice->NumPalDevices() > 1) && (imageMemoryBarrierCount > 0) ?
        virtStackFrame.AllocArray<const Image*>(MaxTransitionCount) : nullptr;

    // True if this barrier needs to wait on prior GPU work
    const bool needExecutionControl =
        (pBarrier->pipePointWaitCount > 1) ||
        (pBarrier->pipePointWaitCount == 1 && pBarrier->pPipePoints[0] != Pal::HwPipeTop) ||
        (pBarrier->gpuEventWaitCount > 0);

    // True if any global cache flushes are being requested
    bool needGlobalCacheFlush = false;
    uint32_t barrierOptions = m_pDevice->GetRuntimeSettings().resourceBarrierOptions;

    for (uint32_t i = 0; i < memBarrierCount; ++i)
    {
        ConvertBarrierCacheFlags(
            m_pDevice,
            pMemoryBarriers[i].srcAccessMask,
            pMemoryBarriers[i].dstAccessMask,
            0xFFFFFFFF, 0xFFFFFFFF,
            barrierOptions,
            pNextMain);

        pNextMain->imageInfo.pImage = nullptr;
        VK_ASSERT(pMemoryBarriers[i].pNext == nullptr);

        needGlobalCacheFlush |= (pNextMain->srcCacheMask != 0);
        ++pNextMain;

        const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

        if (MaxPalAspectsPerMask + mainTransitionCount > MaxTransitionCount)
        {
            FlushBarriers(pBarrier, pTransitions, nullptr, mainTransitionCount, 0, 0);
            pNextMain = pTransitions;
        }
    }

    // If transitioning images out of "srcLayout = UNDEFINED", and the barrier also is required prior to that
    // happening wait for any execution to complete, or sync any caches, then that layout transition must be
    // moved to a separate "post-barrier" that executes after the main barrier.
    //
    // The post barrier is just a barrier executing immediately after the main barrier containing no waits/cache syncs,
    // but just these layout transitions.
    //
    // The reason for this is that PAL always executes all image layout transitions from the Uninitialized
    // (UNDEFINED) layout in the early phase, which executes before any GPU waits or cache syncs.  This could
    // be fixed in PAL, but I think currently this is considered a part of the design.
    bool needPostBarrier = needExecutionControl || needGlobalCacheFlush;

    for (uint32_t i = 0; i < bufferMemoryBarrierCount; ++i)
    {
        const Buffer* pBuffer = Buffer::ObjectFromHandle(pBufferMemoryBarriers[i].buffer);
        uint32_t supportInputCoherMask = pBuffer->GetSupportedInputCoherMask();
        uint32_t supportOutputCoherMask = pBuffer->GetSupportedOutputCoherMask();

        ConvertBarrierCacheFlags(
            m_pDevice,
            pBufferMemoryBarriers[i].srcAccessMask,
            pBufferMemoryBarriers[i].dstAccessMask,
            supportInputCoherMask,
            supportOutputCoherMask,
            barrierOptions,
            pNextMain);

        pNextMain->imageInfo.pImage = nullptr;

        VK_ASSERT(pBufferMemoryBarriers[i].pNext == nullptr);

        ++pNextMain;

        const uint32_t mainTransitionCount = static_cast<uint32_t>(pNextMain - pTransitions);

        if (MaxPalAspectsPerMask + mainTransitionCount > MaxTransitionCount)
        {
            FlushBarriers(pBarrier, pTransitions, nullptr, mainTransitionCount, 0, 0);
            pNextMain = pTransitions;
        }
    }

    uint32_t locationIndex = 0;
    uint32_t locationCount = (imageMemoryBarrierCount > MaxLocationCount) ? MaxLocationCount : imageMemoryBarrierCount;
    Pal::MsaaQuadSamplePattern* pLocations = imageMemoryBarrierCount > 0
                                           ? virtStackFrame.AllocArray<Pal::MsaaQuadSamplePattern>(locationCount)
                                           : nullptr;

    for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i)
    {
        if (pImageMemoryBarriers[i].image == VK_NULL_HANDLE)
        {
            continue;
        }

        const Image*           pImage                 = Image::ObjectFromHandle(pImageMemoryBarriers[i].image);
        VkFormat               format                 = pImage->GetFormat();
        bool                   hasDepthAndStencil     = ((Formats::HasDepth(format)) && (Formats::HasStencil(format)));
        uint32_t               supportInputCoherMask  = pImage->GetSupportedInputCoherMask();
        uint32_t               supportOutputCoherMask = pImage->GetSupportedOutputCoherMask();
        Pal::BarrierTransition barrierTransition      = { 0 };

        ConvertBarrierCacheFlags(
            m_pDevice,
            pImageMemoryBarriers[i].srcAccessMask,
            pImageMemoryBarriers[i].dstAccessMask,
            supportInputCoherMask,
            supportOutputCoherMask,
            barrierOptions,
            &barrierTransition);

        pNextMain->imageInfo.pImage = nullptr;

        uint32_t oldLayoutUsages[MaxRangePerAttachment];
        uint32_t newLayoutUsages[MaxRangePerAttachment];

        VkToPalImageLayoutUsages(pImageMemoryBarriers[i].oldLayout, format, oldLayoutUsages);
        VkToPalImageLayoutUsages(pImageMemoryBarriers[i].newLayout, format, newLayoutUsages);

        Pal::ImageLayout oldLayout = pImage->GetLayoutFromUsage(
            oldLayoutUsages[0],
            this,
            pImageMemoryBarriers[i].srcQueueFamilyIndex);

        Pal::ImageLayout newLayout = pImage->GetLayoutFromUsage(
            newLayoutUsages[0],
            this,
            pImageMemoryBarriers[i].dstQueueFamilyIndex);

        bool layoutChanging = ((oldLayoutUsages[0] != newLayoutUsages[0]) ||
                               (oldLayoutUsages[1] != newLayoutUsages[1]));

        // check if we need to load the sample pattern.
        uint32_t imageSamples = pImage->GetImageSamples();

        if (oldLayout.engines != newLayout.engines)
        {
            // For exclusive sharing access, the application is responsible for inserting two identical "hand-shaking"
            // barriers on each queue's command buffers: one copy to release ownership on the src queue, and another
            // copy to acquire ownership on the destination queue.  We want to execute the layout transition part
            // only on one of them, and we always want to pick the graphics queue.  However, both queues must execute
            // the rest of the barrier.
            if ((pImage->GetSharingMode() == VK_SHARING_MODE_EXCLUSIVE) &&
                (GetPalQueueType() != Pal::QueueTypeUniversal))
            {
                // Skip the layout transition if we are not the graphics queue
                layoutChanging = false;
            }
            else
            {
                layoutChanging = true;
            }
        }

        uint32_t         palRangeIdx   = 0;
        uint32_t         palRangeCount = 0;
        Pal::SubresRange palRanges[MaxPalAspectsPerMask];

        VkToPalSubresRange(
            VkToPalFormat(format).format,
            pImageMemoryBarriers[i].subresourceRange,
            pImage->GetMipLevels(),
            pImage->GetArraySize(),
            palRanges,
            palRangeCount);

        if (hasDepthAndStencil)
        {
            // With both depth and stencil, there should be two ranges
            VK_ASSERT(palRangeCount == MaxPalDepthAspectsPerMask);

            // Combined depth and stencil images may transition independently based on their layouts, so determine
            // the appropriate subset of ranges to transition in case one can be skipped.
            FindDepthStencilLayoutTransitionRanges(
                oldLayoutUsages,
                newLayoutUsages,
                &palRangeIdx,
                &palRangeCount);
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

                if ((hasDepthAndStencil) && (palRangeIdx == 1))
                {
                    // This is the stencil aspect of a combined depth-stencil format.  The two subres ranges may
                    // transition independently, so use the correct layout for stencil.
                    pDestTransition[transitionIdx].imageInfo.oldLayout = pImage->GetLayoutFromUsage(
                        oldLayoutUsages[palRangeIdx],
                        this,
                        pImageMemoryBarrier->srcQueueFamilyIndex);

                    pDestTransition[transitionIdx].imageInfo.newLayout = pImage->GetLayoutFromUsage(
                        newLayoutUsages[palRangeIdx],
                        this,
                        pImageMemoryBarrier->dstQueueFamilyIndex);
                }
                else
                {
                    pDestTransition[transitionIdx].imageInfo.oldLayout = oldLayout;
                    pDestTransition[transitionIdx].imageInfo.newLayout = newLayout;
                }

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
        const uint32_t postTransitionCount = static_cast<uint32_t>(pPostTransitions - pNextPost);

        // Accounting for the maximum sub ranges, do we have enough space left for another image ?
        const bool full = ((MaxPalAspectsPerMask + mainTransitionCount + postTransitionCount) > MaxTransitionCount) ||
                          (locationIndex == locationCount);

        if (full)
        {
            const uint32_t postTransitionStartIdx = static_cast<uint32_t>(pNextPost - pTransitions);

            FlushBarriers(pBarrier, pTransitions, pTransitionImages, mainTransitionCount,
                                                                     postTransitionStartIdx, postTransitionCount);

            // remove any signaled events as we do not want to wait more than once.
            pBarrier->gpuEventWaitCount = 0;
            pBarrier->ppGpuEvents = nullptr;

            pNextMain = pTransitions;
            pNextPost = pPostTransitions;
            locationIndex = 0;
        }
    }

    const uint32_t postTransitionStartIdx = static_cast<uint32_t>(pNextPost - pTransitions);
    const uint32_t mainTransitionCount    = static_cast<uint32_t>(pNextMain - pTransitions);
    const uint32_t postTransitionCount    = static_cast<uint32_t>(pPostTransitions - pNextPost);

    FlushBarriers(pBarrier, pTransitions, pTransitionImages, mainTransitionCount,
                                                             postTransitionStartIdx, postTransitionCount);

    virtStackFrame.FreeArray(pLocations);

    if (pTransitionImages != nullptr)
    {
        virtStackFrame.FreeArray(pTransitionImages);
    }

    virtStackFrame.FreeArray(pTransitions);
}

// =====================================================================================================================
// Implementation of vkCmdWaitEvents()
void CmdBuffer::WaitEvents(
    uint32_t                     eventCount,
    const VkEvent*               pEvents,
    VkPipelineStageFlags         srcStageMask,
    VkPipelineStageFlags         dstStageMask,
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
        // We intentionally ignore the source stage flags as they are irrelevant in the presence of event objects
        VK_IGNORE(srcStageMask);

        barrier.flags.u32All          = 0;
        barrier.reason                = RgpBarrierExternalCmdWaitEvents;
        barrier.waitPoint             = VkToPalWaitPipePoint(dstStageMask);
        barrier.gpuEventWaitCount     = eventCount;
        barrier.ppGpuEvents           = ppGpuEvents;
        barrier.pSplitBarrierGpuEvent = nullptr;

        ExecuteBarriers(virtStackFrame,
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
// Implements vkCmdPipelineBarrier()
void CmdBuffer::PipelineBarrier(
    VkPipelineStageFlags         srcStageMask,
    VkPipelineStageFlags         destStageMask,
    uint32_t                     memBarrierCount,
    const VkMemoryBarrier*       pMemoryBarriers,
    uint32_t                     bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier*  pImageMemoryBarriers)
{
    DbgBarrierPreCmd(DbgBarrierPipelineBarrierWaitEvents);

    VirtualStackFrame virtStackFrame(m_pStackAllocator);

    Pal::BarrierInfo barrier = {};

    // Tell PAL to wait at a specific point until the given set of pipeline events has been signaled (this version
    // does not use GpuEvent objects).
    barrier.reason       = RgpBarrierExternalCmdPipelineBarrier;
    barrier.flags.u32All = 0;
    barrier.waitPoint    = VkToPalWaitPipePoint(destStageMask);

    // Collect signal pipe points.
    Pal::HwPipePoint pipePoints[MaxHwPipePoints];

    barrier.pipePointWaitCount      = VkToPalSrcPipePoints(srcStageMask, pipePoints);
    barrier.pPipePoints             = pipePoints;
    barrier.pSplitBarrierGpuEvent   = nullptr;

    ExecuteBarriers(virtStackFrame,
                    memBarrierCount,
                    pMemoryBarriers,
                    bufferMemoryBarrierCount,
                    pBufferMemoryBarriers,
                    imageMemoryBarrierCount,
                    pImageMemoryBarriers,
                    &barrier);

    DbgBarrierPostCmd(DbgBarrierPipelineBarrierWaitEvents);
}

// =====================================================================================================================
void CmdBuffer::BeginQuery(
    VkQueryPool         queryPool,
    uint32_t            query,
    VkQueryControlFlags flags)
{
    DbgBarrierPreCmd(DbgBarrierQueryBeginEnd);

    const auto palQueryControlFlags = VkToPalQueryControlFlags(flags);

    // NOTE: This function is illegal to call for TimestampQueryPools
    const PalQueryPool* pQueryPool = QueryPool::ObjectFromHandle(queryPool)->AsPalQueryPool();

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdBeginQuery(*pQueryPool->PalPool(deviceIdx),
            pQueryPool->PalQueryType(),
            query,
            palQueryControlFlags);
    }

    const auto* const pRenderPass = m_state.allGpuState.pRenderPass;

    // If queries are used while executing a render pass instance that has multiview enabled,
    // the query uses N consecutive query indices in the query pool (starting at query) where
    // N is the number of bits set in the view mask in the subpass the query is used in.
    //
    // Implementations may write the total result to the first query and
    // write zero to the other queries.
    if (pRenderPass && pRenderPass->IsMultiviewEnabled())
    {
        const auto viewMask  = pRenderPass->GetViewMask(m_renderPassInstance.subpass);
        const auto viewCount = Util::CountSetBits(viewMask);

        // Call Begin() and immediately call End() for all remaining queries,
        // to set value of each remaining query to 0 and to make them avaliable.
        for (uint32_t remainingQuery = 1; remainingQuery < viewCount; ++remainingQuery)
        {
            const auto remainingQueryIndex = query + remainingQuery;

            deviceGroup = utils::IterateMask(m_palDeviceMask);
            while (deviceGroup.Iterate())
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdBeginQuery(
                    *pQueryPool->PalPool(deviceIdx),
                    pQueryPool->PalQueryType(),
                    remainingQueryIndex, palQueryControlFlags);

                PalCmdBuffer(deviceIdx)->CmdEndQuery(
                    *pQueryPool->PalPool(deviceIdx),
                    pQueryPool->PalQueryType(),
                    remainingQueryIndex);
            }
        }
    }

    DbgBarrierPostCmd(DbgBarrierQueryBeginEnd);
}

// =====================================================================================================================
void CmdBuffer::EndQuery(
    VkQueryPool queryPool,
    uint32_t    query)
{
    DbgBarrierPreCmd(DbgBarrierQueryBeginEnd);

    // NOTE: This function is illegal to call for TimestampQueryPools
    const PalQueryPool* pQueryPool = QueryPool::ObjectFromHandle(queryPool)->AsPalQueryPool();

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdEndQuery(*pQueryPool->PalPool(deviceIdx),
            pQueryPool->PalQueryType(),
            query);
    }

    DbgBarrierPostCmd(DbgBarrierQueryBeginEnd);
}

void CmdBuffer::FillTimestampQueryPool(
    const TimestampQueryPool& timestampQueryPool,
    const uint32_t firstQuery,
    const uint32_t queryCount,
    const uint32_t timestampChunk)
{
    // All the cache operations operating on the query pool's timestamp memory
    // that may have occurred before/after this reset.
    static const uint32_t TimestampCoher =
        Pal::CoherShader    | // vkCmdCopyQueryPoolResults (CmdDispatch)
        Pal::CoherMemory    | // vkCmdResetQueryPool (CmdFillMemory)
        Pal::CoherTimestamp;  // vkCmdWriteTimestamp (CmdWriteTimestamp)

    static const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;
    static const Pal::BarrierFlags flags = { 0 };

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
            flags,                                  // flags
            Pal::HwPipeTop,                         // waitPoint
            1,                                      // pipePointWaitCount
            &pipePoint,                             // pPipePoints
            0,                                      // gpuEventCount
            nullptr,                                // ppGpuEvents
            0,                                      // rangeCheckedTargetWaitCount
            nullptr,                                // ppTargets
            1,                                      // transitionCount
            &Transition,                            // pTransitions
            nullptr,                                // pSplitBarrierGpuEvent
            RgpBarrierInternalPreResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier);
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

    utils::IterateMask deviceGroup1(m_palDeviceMask);
    while (deviceGroup1.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup1.Index();

        PalCmdBuffer(deviceIdx)->CmdFillMemory(
            timestampQueryPool.PalMemory(deviceIdx),
            timestampQueryPool.GetSlotOffset(firstQuery),
            TimestampQueryPool::SlotSize * queryCount,
            timestampChunk);
    }

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
            flags,                                   // flags
            Pal::HwPipeTop,                          // waitPoint
            1,                                       // pipePointWaitCount
            &pipePoint,                              // pPipePoints
            0,                                       // gpuEventCount
            nullptr,                                 // ppGpuEvents
            0,                                       // rangeCheckedTargetWaitCount
            nullptr,                                 // ppTargets
            1,                                       // transitionCount
            &Transition,                             // pTransitions
            nullptr,                                 // pSplitBarrierGpuEvent
            RgpBarrierInternalPostResetQueryPoolSync // reason
        };

        PalCmdBarrier(Barrier);
    }
}

// =====================================================================================================================
void CmdBuffer::ResetQueryPool(
    VkQueryPool queryPool,
    uint32_t    firstQuery,
    uint32_t    queryCount)
{
    DbgBarrierPreCmd(DbgBarrierQueryReset);

    const QueryPool* pBasePool = QueryPool::ObjectFromHandle(queryPool);

    if (pBasePool->GetQueryType() != VK_QUERY_TYPE_TIMESTAMP)
    {
        const PalQueryPool* pQueryPool = pBasePool->AsPalQueryPool();

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            PalCmdBuffer(deviceIdx)->CmdResetQueryPool(
                *pQueryPool->PalPool(deviceIdx),
                firstQuery,
                queryCount);
        }
    }
    else
    {
        const TimestampQueryPool* pQueryPool = pBasePool->AsTimestampQueryPool();

        // Write TimestampNotReady to all timestamps in TimestampQueryPool.
        FillTimestampQueryPool(
           *pQueryPool,
            firstQuery,
            queryCount,
            TimestampQueryPool::TimestampNotReadyChunk);
    }

    DbgBarrierPostCmd(DbgBarrierQueryReset);
}

// =====================================================================================================================
// This is the main hook for any CmdBarrier going into PAL.  Always call this function instead of CmdBarrier directly.
void CmdBuffer::PalCmdBarrier(
    const Pal::BarrierInfo& info)
{
    // If you trip this assert, you've forgotten to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block your main code change.
    VK_ASSERT(info.reason != 0);

#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32_t i = 0; i < info.transitionCount; ++i)
    {
        // Detect if PAL may execute a barrier blt using this image
        const auto& transInfo = info.pTransitions[i].imageInfo;

        VK_ASSERT(info.pTransitions[i].imageInfo.pImage == nullptr);
        // You need to use the other PalCmdBarrier method (below) which uses vk::Image ptrs to obtain the
        // corresponding Pal::IImage ptr for each image transition
    }
#endif

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdBarrier(info);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdBarrier(
    Pal::BarrierInfo*             pInfo,
    Pal::BarrierTransition* const pTransitions,
    const Image** const           pTransitionImages)
{
    // If you trip this assert, you've forgot to populate a value for this field.  You should use one of the
    // RgpBarrierReason enum values from sqtt_rgp_annotations.h.  Preferably you should add a new one as described
    // in the header, but temporarily you may use the generic "unknown" reason so as not to block you.
    VK_ASSERT(pInfo->reason != 0);

    const Pal::IGpuEvent** ppOriginalGpuEvents = pInfo->ppGpuEvents;

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
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
}

// =====================================================================================================================
void CmdBuffer::PalCmdBindMsaaStates(
    const Pal::IMsaaState* const * pStates)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBindMsaaState(PalCmdBuffer(deviceIdx), deviceIdx, (pStates != nullptr) ? pStates[deviceIdx] : nullptr);
    }
}

// =====================================================================================================================
void CmdBuffer::PalCmdSetMsaaQuadSamplePattern(
    uint32_t                          numSamplesPerPixel,
    const Pal::MsaaQuadSamplePattern& quadSamplePattern)
{
    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        PalCmdBuffer(deviceIdx)->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
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

    const QueryPool* pBasePool = QueryPool::ObjectFromHandle(queryPool);
    const Buffer* pDestBuffer = Buffer::ObjectFromHandle(destBuffer);

    if (pBasePool->GetQueryType() != VK_QUERY_TYPE_TIMESTAMP)
    {
        const PalQueryPool* pPool = pBasePool->AsPalQueryPool();

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
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
    }
    else
    {
        const TimestampQueryPool* pPool = pBasePool->AsTimestampQueryPool();

        const Device::InternalPipeline& pipeline = m_pDevice->GetTimestampQueryCopyPipeline();

        // Wait for all previous query timestamps to complete.  For now we have to do a full pipeline idle but once
        // we have a PAL interface for doing a 64-bit WAIT_REG_MEM, we only have to wait on the queries being copied
        // here
        if ((flags & VK_QUERY_RESULT_WAIT_BIT) != 0)
        {
            static const Pal::BarrierTransition transition =
            {
                Pal::CoherTimestamp,
                Pal::CoherShader
            };

            static const Pal::HwPipePoint pipePoint = Pal::HwPipeBottom;
            static const Pal::BarrierFlags PalBarrierFlags = {0};

            static const Pal::BarrierInfo TimestampWriteWaitIdle =
            {
                PalBarrierFlags,                                // flags
                Pal::HwPipePreCs,                               // waitPoint
                1,                                              // pipePointWaitCount
                &pipePoint,                                     // pPipePoints
                0,                                              // gpuEventWaitCount
                nullptr,                                        // ppGpuEvents
                0,                                              // rangeCheckedTargetWaitCount
                nullptr,                                        // ppTargets
                1,                                              // transitionCount
                &transition,                                    // pTransitions
                nullptr,                                        // pSplitBarrierGpuEvent
                RgpBarrierInternalPreCopyQueryPoolResultsSync   // reason
            };

            PalCmdBarrier(TimestampWriteWaitIdle);
        }

        uint32_t userData[16];

        // Figure out which user data registers should contain what compute constants
        const uint32_t storageViewSize     = m_pDevice->GetProperties().descriptorSizes.bufferView;
        const uint32_t storageViewDwSize   = storageViewSize / sizeof(uint32_t);
        const uint32_t timestampViewOffset = 0;
        const uint32_t bufferViewOffset    = storageViewDwSize;
        const uint32_t queryCountOffset    = bufferViewOffset + storageViewDwSize;
        const uint32_t copyFlagsOffset     = queryCountOffset + 1;
        const uint32_t copyStrideOffset    = copyFlagsOffset  + 1;
        const uint32_t firstQueryOffset    = copyStrideOffset + 1;
        const uint32_t userDataCount       = firstQueryOffset + 1;

        // Make sure they agree with pipeline mapping
        VK_ASSERT(timestampViewOffset == pipeline.userDataNodeOffsets[0]);
        VK_ASSERT(bufferViewOffset    == pipeline.userDataNodeOffsets[1]);
        VK_ASSERT(queryCountOffset    == pipeline.userDataNodeOffsets[2]);
        VK_ASSERT(userDataCount <= VK_ARRAY_SIZE(userData));

        // Create and set a raw storage view into the destination buffer (shader will choose to either write 32-bit or
        // 64-bit values)
        Pal::BufferViewInfo bufferViewInfo = {};

        bufferViewInfo.range          = destStride * queryCount;
        bufferViewInfo.stride         = 0; // Raw buffers have a zero byte stride
        bufferViewInfo.swizzledFormat = Pal::UndefinedSwizzledFormat;

        // Set query count
        userData[queryCountOffset] = queryCount;

        // These are magic numbers that match literal values in the shader
        constexpr uint32_t Copy64Bit                  = 0x1;
        constexpr uint32_t CopyIncludeAvailabilityBit = 0x2;

        // Set copy flags
        userData[copyFlagsOffset]  = 0;
        userData[copyFlagsOffset] |= (flags & VK_QUERY_RESULT_64_BIT) ? Copy64Bit : 0x0;
        userData[copyFlagsOffset] |= (flags & VK_QUERY_RESULT_WITH_AVAILABILITY_BIT) ? CopyIncludeAvailabilityBit : 0x0;

        // Set destination stride
        VK_ASSERT(destStride <= UINT_MAX); // TODO: Do we really need to handle this?

        userData[copyStrideOffset] = static_cast<uint32_t>(destStride);

        // Set start query index
        userData[firstQueryOffset] = firstQuery;

        utils::IterateMask deviceGroup(m_palDeviceMask);
        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            // Backup PAL compute state
            PalCmdBuffer(deviceIdx)->CmdSaveComputeState(Pal::ComputeStatePipelineAndUserData);

            // Bind the copy compute pipeline
            PalCmdBuffer(deviceIdx)->CmdBindPipeline({Pal::PipelineBindPoint::Compute,
                                                      pipeline.pPipeline[deviceIdx],});

            // Set the timestamp buffer SRD (copy source) as typed 64-bit storage view
            memcpy(&userData[timestampViewOffset], pPool->GetStorageView(deviceIdx), storageViewSize);

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

            PalCmdBuffer(deviceIdx)->CmdDispatch(threadGroupCount, 1, 1);

            // Restore compute state
            PalCmdBuffer(deviceIdx)->CmdRestoreComputeState(Pal::ComputeStatePipelineAndUserData);

            // Note that the application is responsible for doing a post-copy sync using a barrier.
        }
    }

    DbgBarrierPostCmd(DbgBarrierCopyBuffer | DbgBarrierCopyQueryPool);
}

// =====================================================================================================================
// Command to write a timestamp value to a location in a Timestamp query pool
void CmdBuffer::WriteTimestamp(
    VkPipelineStageFlagBits   pipelineStage,
    const TimestampQueryPool* pQueryPool,
    uint32_t                  query)
{
    DbgBarrierPreCmd(DbgBarrierWriteTimestamp);

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdWriteTimestamp(
            VkToPalSrcPipePointForTimestampWrite(pipelineStage),
            pQueryPool->PalMemory(deviceIdx),
            pQueryPool->GetSlotOffset(query));

        const auto* const pRenderPass = m_state.allGpuState.pRenderPass;

        // If vkCmdWriteTimestamp is called while executing a render pass instance that has multiview enabled,
        // the timestamp uses N consecutive query indices in the query pool (starting at query) where
        // N is the number of bits set in the view mask of the subpass the command is executed in.
        //
        // The first query is a timestamp value and (if more than one bit is set in the view mask)
        // zero is written to the remaining queries.
        if (pRenderPass && pRenderPass->IsMultiviewEnabled())
        {
            const auto viewMask  = pRenderPass->GetViewMask(m_renderPassInstance.subpass);
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
    DbgBarrierPostCmd(DbgBarrierWriteTimestamp);
}

// =====================================================================================================================
void CmdBuffer::SetSampleLocations(
    const VkSampleLocationsInfoEXT* pSampleLocationsInfo)
{
    VK_ASSERT((m_state.allGpuState.pGraphicsPipeline != nullptr) &&
        (m_state.allGpuState.pGraphicsPipeline->PipelineSetsState(
            DynamicStatesInternal::SAMPLE_LOCATIONS_EXT) == false));

    Pal::MsaaQuadSamplePattern locations;
    uint32_t sampleLocationsPerPixel = (uint32_t)pSampleLocationsInfo->sampleLocationsPerPixel;

    ConvertToPalMsaaQuadSamplePattern(pSampleLocationsInfo, &locations);
    PalCmdSetMsaaQuadSamplePattern(sampleLocationsPerPixel, locations);
}

// =====================================================================================================================
// Programs the current GPU sample pattern to the one belonging to the given subpass in a current render pass instance
void CmdBuffer::RPInitSamplePattern()
{
    const RenderPass* pRenderPass = m_state.allGpuState.pRenderPass;

    if (pRenderPass->GetAttachmentCount() > 0)
    {
        const SamplePattern* pSamplePattern = &m_renderPassInstance.pSamplePatterns[0];

        if (pSamplePattern->sampleCount > 0)
        {
            PalCmdSetMsaaQuadSamplePattern(
                pSamplePattern->sampleCount,
                pSamplePattern->locations);
        }
    }
}

// =====================================================================================================================
// Begins a render pass instance (vkCmdBeginRenderPass)
void CmdBuffer::BeginRenderPass(
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents            contents)
{
    DbgBarrierPreCmd(DbgBarrierBeginRenderPass);

    m_state.allGpuState.pRenderPass  = RenderPass::ObjectFromHandle(pRenderPassBegin->renderPass);
    m_state.allGpuState.pFramebuffer = Framebuffer::ObjectFromHandle(pRenderPassBegin->framebuffer);

    Pal::Result result = Pal::Result::Success;

    EXTRACT_VK_STRUCTURES_2(
        RP,
        RenderPassBeginInfo,
        DeviceGroupRenderPassBeginInfoKHX,
        RenderPassSampleLocationsBeginInfoEXT,
        pRenderPassBegin,
        RENDER_PASS_BEGIN_INFO,
        DEVICE_GROUP_RENDER_PASS_BEGIN_INFO_KHX,
        RENDER_PASS_SAMPLE_LOCATIONS_BEGIN_INFO_EXT)

    // Copy render areas (these may be per-device in a group)
    bool replicateRenderArea = true;

    if (pDeviceGroupRenderPassBeginInfoKHX != nullptr)
    {
        replicateRenderArea = (pDeviceGroupRenderPassBeginInfoKHX->deviceRenderAreaCount == 0);

        SetDeviceMask(pDeviceGroupRenderPassBeginInfoKHX->deviceMask);

        m_renderPassInstance.renderAreaCount = pDeviceGroupRenderPassBeginInfoKHX->deviceRenderAreaCount;

        VK_ASSERT(m_renderPassInstance.renderAreaCount <= MaxPalDevices);

        utils::IterateMask deviceGroup(pDeviceGroupRenderPassBeginInfoKHX->deviceMask);

        for (uint32_t areaIdx = 0; areaIdx < pDeviceGroupRenderPassBeginInfoKHX->deviceRenderAreaCount; areaIdx++)
        {
            const VkRect2D& srcRect = pDeviceGroupRenderPassBeginInfoKHX->pDeviceRenderAreas[areaIdx];

            deviceGroup.Iterate();

            const uint32_t deviceIdx = deviceGroup.Index();

            auto* pDstRect = &m_renderPassInstance.renderArea[deviceIdx];

            pDstRect->offset.x      = srcRect.offset.x;
            pDstRect->offset.y      = srcRect.offset.y;
            pDstRect->extent.width  = srcRect.extent.width;
            pDstRect->extent.height = srcRect.extent.height;
        }

        VK_ASSERT(deviceGroup.Count() == pDeviceGroupRenderPassBeginInfoKHX->deviceRenderAreaCount);
    }

    if (replicateRenderArea)
    {
        m_renderPassInstance.renderAreaCount = m_pDevice->NumPalDevices();

        const auto& srcRect = pRenderPassBeginInfo->renderArea;

        for (uint32_t deviceIdx = 0; deviceIdx <  m_pDevice->NumPalDevices(); deviceIdx++)
        {
            auto* pDstRect          = &m_renderPassInstance.renderArea[deviceIdx];

            pDstRect->offset.x      = srcRect.offset.x;
            pDstRect->offset.y      = srcRect.offset.y;
            pDstRect->extent.width  = srcRect.extent.width;
            pDstRect->extent.height = srcRect.extent.height;
        }
    }

    const uint32_t attachmentCount = m_state.allGpuState.pRenderPass->GetAttachmentCount();

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
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    const uint32_t subpassCount = m_state.allGpuState.pRenderPass->GetSubpassCount();

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

        memset(m_renderPassInstance.pSamplePatterns, 0, subpassCount * sizeof(SamplePattern));

        if (m_renderPassInstance.pSamplePatterns != nullptr)
        {
            m_renderPassInstance.maxSubpassCount = subpassCount;
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
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
            const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(a);
            const Pal::ImageAspect firstAspect        = attachment.subresRange[0].startSubres.aspect;

            const RPImageLayout initialLayout =
            { m_state.allGpuState.pRenderPass->GetAttachmentDesc(a).initialLayout, 0 };

            if (firstAspect == Pal::ImageAspect::Color)
            {
                RPSetAttachmentLayout(
                    a,
                    Pal::ImageAspect::Color,
                    attachment.pImage->GetAttachmentLayout(initialLayout, Pal::ImageAspect::Color, this));

                RPSetAttachmentLayout(a, Pal::ImageAspect::Depth, NullLayout);
                RPSetAttachmentLayout(a, Pal::ImageAspect::Stencil, NullLayout);
            }
            else
            {
                // Note that we set both depth and stencil aspect layouts for depth/stencil formats to define
                // initial values for them.  This avoids some (incorrect) PAL asserts when clearing depth- or
                // stencil-only surfaces.  Here, the missing aspect will have a null usage but a non-null engine
                // component.
                VK_ASSERT((firstAspect == Pal::ImageAspect::Depth) ||
                            (firstAspect == Pal::ImageAspect::Stencil));

                RPSetAttachmentLayout(a, Pal::ImageAspect::Color, NullLayout);

                RPSetAttachmentLayout(
                    a,
                    Pal::ImageAspect::Depth,
                    attachment.pImage->GetAttachmentLayout(initialLayout, Pal::ImageAspect::Depth, this));

                RPSetAttachmentLayout(
                    a,
                    Pal::ImageAspect::Stencil,
                    attachment.pImage->GetAttachmentLayout(initialLayout, Pal::ImageAspect::Stencil, this));
            }
        }

        for (uint32_t subpassIndex = 0; subpassIndex < subpassCount; subpassIndex++)
        {
            const uint32_t subpassMaxSampleCount =
                m_state.allGpuState.pRenderPass->GetSubpassMaxSampleCount(m_renderPassInstance.subpass);

            if (subpassMaxSampleCount > 0)
            {
                m_renderPassInstance.pSamplePatterns[subpassIndex].sampleCount = subpassMaxSampleCount;
                m_renderPassInstance.pSamplePatterns[subpassIndex].locations =
                    *Device::GetDefaultQuadSamplePattern(subpassMaxSampleCount);
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
                    m_state.allGpuState.pFramebuffer->GetAttachment(attachmentIndex);

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

        // Initialize sample pattern
        RPInitSamplePattern();

        // Begin the first subpass
        m_renderPassInstance.pExecuteInfo = m_state.allGpuState.pRenderPass->GetExecuteInfo();

        RPBeginSubpass();
    }
    else
    {
        // Set a dummy state such that other instance commands ignore the render pass instance.
        m_renderPassInstance.subpass = VK_SUBPASS_EXTERNAL;
    }

    DbgBarrierPostCmd(DbgBarrierBeginRenderPass);}

// =====================================================================================================================
// Advances to the next sub-pass in the current render pass (vkCmdNextSubPass)
void CmdBuffer::NextSubPass(VkSubpassContents contents)
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
    VK_ASSERT(m_renderPassInstance.subpass < m_state.allGpuState.pRenderPass->GetSubpassCount());

    // Get current subpass execute state
    const auto& subpass = m_renderPassInstance.pExecuteInfo->pSubpasses[m_renderPassInstance.subpass];

    VirtualStackFrame virtStack(m_pStackAllocator);

    // Synchronize preceding work before resolving if needed
    if (subpass.end.flags.hasPreResolveSyncPoint)
    {
        RPSyncPoint(subpass.end.syncPreResolve, &virtStack);
    }

    // Execute any multisample resolve attachment operations
    if (subpass.end.resolveCount > 0)
    {
        RPResolveAttachments(subpass.end.resolveCount, subpass.end.pResolves);
    }

    // Synchronize preceding work at the end of the subpass
    if (subpass.end.flags.hasBottomSyncPoint)
    {
        RPSyncPoint(subpass.end.syncBottom, &virtStack);
    }
}

// =====================================================================================================================
// Begins the current subpass during a render pass instance.
void CmdBuffer::RPBeginSubpass()
{
    VK_ASSERT(m_renderPassInstance.subpass < m_state.allGpuState.pRenderPass->GetSubpassCount());

    VirtualStackFrame virtStack(m_pStackAllocator);

    // Get current subpass execute state
    const auto& subpass = m_renderPassInstance.pExecuteInfo->pSubpasses[m_renderPassInstance.subpass];

    // Synchronize prior work (defined by subpass dependencies) prior to the top of this subpass, and handle any
    // layout transitions for this subpass's references.
    if (subpass.begin.flags.hasTopSyncPoint)
    {
        RPSyncPoint(subpass.begin.syncTop, &virtStack);
    }

    // Execute any color clear load operations
    if (subpass.begin.loadOps.colorClearCount > 0)
    {
        RPLoadOpClearColor(subpass.begin.loadOps.colorClearCount, subpass.begin.loadOps.pColorClears);
    }

    // Execute any depth-stencil clear load operations
    if (subpass.begin.loadOps.dsClearCount > 0)
    {
        RPLoadOpClearDepthStencil(subpass.begin.loadOps.dsClearCount, subpass.begin.loadOps.pDsClears);
    }

    // Bind targets
    RPBindTargets(subpass.begin.bindTargets);

    // Set view instance mask
    SetViewInstanceMask();
}

// =====================================================================================================================
// Executes a "sync point" during a render pass instance.  There are a number of these at different stages between
// subpasses where we handle execution/memory dependencies from subpass dependencies as well as trigger automatic
// layout transitions.
void CmdBuffer::RPSyncPoint(
    const RPSyncPointInfo& syncPoint,
    VirtualStackFrame*     pVirtStack)
{
    const uint32_t barrierOptions = m_pDevice->GetRuntimeSettings().resourceBarrierOptions;

    Pal::BarrierInfo barrier = {};

    barrier.reason    = RgpBarrierExternalRenderPassSync;
    barrier.waitPoint = Pal::HwPipeBottom;

    // Get the PAL wait point for the barrier based on the subpass dependency
    if (syncPoint.barrier.dstStageMask != 0)
    {
        barrier.waitPoint = VkToPalWaitPipePoint(syncPoint.barrier.dstStageMask);
    }

    // Get the PAL signal pipe points based on the subpass dependency
    Pal::HwPipePoint pipePoints[MaxHwPipePoints + 2]; // Two extra for pre-resolve and post-resolve signals

    barrier.pipePointWaitCount = VkToPalSrcPipePoints(syncPoint.barrier.srcStageMask, pipePoints);
    barrier.pPipePoints        = pipePoints;

    // Augment the active source pipeline stages for resolves if we need to wait for prior resolves to complete
    if (syncPoint.barrier.flags.postResolveSync)
    {
        VK_ASSERT(barrier.pipePointWaitCount < VK_ARRAY_SIZE(pipePoints));

        pipePoints[barrier.pipePointWaitCount++] = Pal::HwPipePostBlt;

        // If there is no actual explicit waiting done for other reasons (e.g. dependency) we need to wait at the
        // top of the pipe here.
        if (barrier.waitPoint == Pal::HwPipeBottom)
        {
            barrier.waitPoint = Pal::HwPipeTop;
        }
    }

    // Similarly augment the waiting if we need to wait for prior color rendering to finish
    if (syncPoint.barrier.flags.preColorResolveSync ||
        syncPoint.barrier.flags.preDsResolveSync)
    {
        VK_ASSERT(barrier.pipePointWaitCount < VK_ARRAY_SIZE(pipePoints));

        pipePoints[barrier.pipePointWaitCount++] = Pal::HwPipeBottom;

        // If we're waiting prior a resolve, make sure the wait point waits early enough.
        if (barrier.waitPoint > Pal::HwPipePreBlt)
        {
            barrier.waitPoint = Pal::HwPipePreBlt;
        }
    }

    const uint32_t maxTransitionCount = 1 + // For global memory dependency
                                        MaxRangePerAttachment * syncPoint.transitionCount;

    Pal::BarrierTransition* pPalTransitions = pVirtStack->AllocArray<Pal::BarrierTransition>(maxTransitionCount);
    const Image** ppImages                  = pVirtStack->AllocArray<const Image*>(maxTransitionCount);

    if ((pPalTransitions != nullptr) && (ppImages != nullptr))
    {
        // Construct global memory dependency to synchronize caches
        if (syncPoint.barrier.srcAccessMask != 0        ||
            syncPoint.barrier.dstAccessMask != 0        ||
            syncPoint.barrier.flags.preColorResolveSync ||
            syncPoint.barrier.flags.preDsResolveSync    ||
            syncPoint.barrier.flags.postResolveSync)
        {
            VK_ASSERT(barrier.transitionCount < maxTransitionCount);

            Pal::BarrierTransition* pGlobalTransition = &pPalTransitions[barrier.transitionCount++];

            pGlobalTransition->imageInfo.pImage = nullptr;

            ConvertBarrierCacheFlags(
                m_pDevice,
                syncPoint.barrier.srcAccessMask,
                syncPoint.barrier.dstAccessMask,
                0xffffffff,
                0xffffffff,
                barrierOptions,
                pGlobalTransition);

            if (syncPoint.barrier.flags.preColorResolveSync)
            {
                pGlobalTransition->srcCacheMask |= Pal::CoherColorTarget;
                pGlobalTransition->dstCacheMask |= Pal::CoherResolve;
            }

            if (syncPoint.barrier.flags.preDsResolveSync)
            {
                pGlobalTransition->srcCacheMask |= Pal::CoherDepthStencilTarget;
                pGlobalTransition->dstCacheMask |= Pal::CoherResolve;
            }

            if (syncPoint.barrier.flags.postResolveSync)
            {
                pGlobalTransition->srcCacheMask |= Pal::CoherResolve;
            }
        }

        // Construct attachment-specific layout transitions
        for (uint32_t t = 0; t < syncPoint.transitionCount; ++t)
        {
            const RPTransitionInfo& tr = syncPoint.pTransitions[t];

            const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(tr.attachment);

            for (uint32_t sr = 0; sr < attachment.subresRangeCount; ++sr)
            {
                const Pal::ImageAspect aspect = attachment.subresRange[sr].startSubres.aspect;

                const Pal::ImageLayout newLayout = attachment.pImage->GetAttachmentLayout(
                    tr.nextLayout,
                    aspect,
                    this);

                const Pal::ImageLayout oldLayout = RPGetAttachmentLayout(
                    tr.attachment,
                    aspect);

                if (oldLayout.usages  != newLayout.usages ||
                    oldLayout.engines != newLayout.engines)
                {
                    VK_ASSERT(barrier.transitionCount < maxTransitionCount);

                    ppImages[barrier.transitionCount]         = attachment.pImage;

                    Pal::BarrierTransition* pLayoutTransition = &pPalTransitions[barrier.transitionCount++];

                    pLayoutTransition->srcCacheMask                         = 0;
                    pLayoutTransition->dstCacheMask                         = 0;
                    pLayoutTransition->imageInfo.pImage                     = attachment.pImage->PalImage();
                    pLayoutTransition->imageInfo.oldLayout                  = oldLayout;
                    pLayoutTransition->imageInfo.newLayout                  = newLayout;
                    pLayoutTransition->imageInfo.subresRange                = attachment.subresRange[sr];
                    pLayoutTransition->imageInfo.pQuadSamplePattern         = 0;

                    const uint32_t sampleCount = attachment.pImage->GetImageSamples();

                    if (sampleCount > 1)
                    {
                        const Pal::MsaaQuadSamplePattern* pQuadSamplePattern = pLayoutTransition->imageInfo.pQuadSamplePattern;

                         if (attachment.pImage->IsSampleLocationsCompatibleDepth() &&
                             tr.flags.isInitialLayoutTransition)
                         {
                             VK_ASSERT(Formats::HasDepth(attachment.pImage->GetFormat()));

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
                    }

                    RPSetAttachmentLayout(tr.attachment, aspect, newLayout);
                }
            }
        }

        barrier.pTransitions = pPalTransitions;
    }
    else
    {
        m_recordingResult = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Execute the barrier if it actually did anything
    if ((barrier.waitPoint != Pal::HwPipeBottom) ||
        (barrier.transitionCount > 0) ||
        ((barrier.pipePointWaitCount > 1) ||
         (barrier.pipePointWaitCount == 1 && barrier.pPipePoints[0] != Pal::HwPipeTop)))
    {
        PalCmdBarrier(&barrier, pPalTransitions, ppImages);
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
// Does one or more load-op color clears during a render pass instance.
void CmdBuffer::RPLoadOpClearColor(
    uint32_t                 count,
    const RPLoadOpClearInfo* pClears)
{
    if (m_pSqttState != nullptr)
    {
        m_pSqttState->BeginRenderPassColorClear();
    }

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPLoadOpClearInfo& clear = pClears[i];

        VK_ASSERT(clear.aspect == VK_IMAGE_ASPECT_COLOR_BIT);

        const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(clear.attachment);
        const Pal::ImageLayout clearLayout        = RPGetAttachmentLayout(clear.attachment, Pal::ImageAspect::Color);

        VK_ASSERT(clearLayout.usages & Pal::LayoutColorTarget);

        // Convert the clear color to the format of the attachment view
        Pal::ClearColor clearColor = VkToPalClearColor(
            &m_renderPassInstance.pAttachments[clear.attachment].clearValue.color,
            attachment.viewFormat.format);

        const auto clearSubresRanges = LoadOpClearSubresRanges(
            attachment, clear,
            *m_state.allGpuState.pRenderPass);

        utils::IterateMask deviceGroup(GetDeviceMask());

        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            Pal::Box clearBox = BuildClearBox(m_renderPassInstance.renderArea[deviceIdx], attachment);

            PalCmdBuffer(deviceIdx)->CmdClearColorImage(
                *attachment.pImage->PalImage(deviceIdx),
                clearLayout,
                clearColor,
                clearSubresRanges.NumElements(),
                clearSubresRanges.Data(),
                1, &clearBox,
                Pal::ColorClearAutoSync);
        }
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

    for (uint32_t i = 0; i < count; ++i)
    {
        const RPLoadOpClearInfo& clear = pClears[i];

        VK_ASSERT((clear.aspect & VK_IMAGE_ASPECT_COLOR_BIT) == 0);
        VK_ASSERT(clear.aspect != 0);

        const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(clear.attachment);

        const Pal::ImageLayout depthLayout   = RPGetAttachmentLayout(clear.attachment, Pal::ImageAspect::Depth);
        const Pal::ImageLayout stencilLayout = RPGetAttachmentLayout(clear.attachment, Pal::ImageAspect::Stencil);

        // Convert the clear color to the format of the attachment view
        const VkClearValue& clearValue = m_renderPassInstance.pAttachments[clear.attachment].clearValue;

        float clearDepth        = VkToPalClearDepth(clearValue.depthStencil.depth);
        Pal::uint8 clearStencil = clearValue.depthStencil.stencil;

        const auto clearSubresRanges = LoadOpClearSubresRanges(
            attachment, clear,
            *m_state.allGpuState.pRenderPass);

        utils::IterateMask deviceGroup(GetDeviceMask());

        while (deviceGroup.Iterate())
        {
            const uint32_t deviceIdx = deviceGroup.Index();

            const Pal::Rect& clearRect = m_renderPassInstance.renderArea[deviceIdx];

            PalCmdBuffer(deviceIdx)->CmdClearDepthStencil(
                *attachment.pImage->PalImage(deviceIdx),
                depthLayout,
                stencilLayout,
                clearDepth,
                clearStencil,
                clearSubresRanges.NumElements(),
                clearSubresRanges.Data(),
                1, &clearRect,
                Pal::DsClearAutoSync);
        }
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
            m_state.allGpuState.pFramebuffer->GetAttachment(params.src.attachment);
        const Framebuffer::Attachment& dstAttachment =
            m_state.allGpuState.pFramebuffer->GetAttachment(params.dst.attachment);

        // Both color and depth-stencil resolves are allowed by resolve attachments
        // SubresRange shall be exactly same for src and dst.
        VK_ASSERT(srcAttachment.subresRangeCount == dstAttachment.subresRangeCount);
        VK_ASSERT(srcAttachment.subresRange[0].numMips == 1);

        const uint32_t sliceCount = Util::Min(
            srcAttachment.subresRange[0].numSlices,
            dstAttachment.subresRange[0].numSlices);

        // During split-frame-rendering, the image to resolve could be split across multiple devices.
        Pal::ImageResolveRegion regions[MaxPalDevices][MaxRangePerAttachment];

        // We expect MSAA images to never have mipmaps
        VK_ASSERT(srcAttachment.subresRange[0].startSubres.mipLevel == 0);

        uint32_t         aspectRegionCount                     = 0;
        Pal::ImageAspect resolveAspects[MaxRangePerAttachment] = {};
        const VkFormat   resolveFormat                         = srcAttachment.pView->GetViewFormat();

        if (Formats::IsDepthStencilFormat(resolveFormat) == false)
        {
            resolveAspects[0] = Pal::ImageAspect::Color;
            aspectRegionCount = 1;
        }
        else
        {
            if (Formats::HasDepth(resolveFormat))
            {
                resolveAspects[aspectRegionCount++] = Pal::ImageAspect::Depth;
            }

            if (Formats::HasStencil(resolveFormat))
            {
                resolveAspects[aspectRegionCount++] = Pal::ImageAspect::Stencil;
            }
        }

        const Pal::ImageLayout srcLayout = RPGetAttachmentLayout(params.src.attachment, resolveAspects[0]);
        const Pal::ImageLayout dstLayout = RPGetAttachmentLayout(params.dst.attachment, resolveAspects[0]);

        for (uint32_t idx = 0; idx < m_renderPassInstance.renderAreaCount; idx++)
        {
            for (uint32_t aspectRegionIndex = 0; aspectRegionIndex < aspectRegionCount; ++aspectRegionIndex)
            {
                const Pal::Rect& renderArea = m_renderPassInstance.renderArea[idx];

                regions[idx][aspectRegionIndex].srcAspect      = resolveAspects[aspectRegionIndex];
                regions[idx][aspectRegionIndex].srcSlice       = srcAttachment.subresRange[0].startSubres.arraySlice;
                regions[idx][aspectRegionIndex].srcOffset.x    = renderArea.offset.x;
                regions[idx][aspectRegionIndex].srcOffset.y    = renderArea.offset.y;
                regions[idx][aspectRegionIndex].srcOffset.z    = 0;
                regions[idx][aspectRegionIndex].dstAspect      = resolveAspects[aspectRegionIndex];
                regions[idx][aspectRegionIndex].dstMipLevel    = dstAttachment.subresRange[0].startSubres.mipLevel;
                regions[idx][aspectRegionIndex].dstSlice       = dstAttachment.subresRange[0].startSubres.arraySlice;
                regions[idx][aspectRegionIndex].dstOffset.x    = renderArea.offset.x;
                regions[idx][aspectRegionIndex].dstOffset.y    = renderArea.offset.y;
                regions[idx][aspectRegionIndex].dstOffset.z    = 0;
                regions[idx][aspectRegionIndex].extent.width   = renderArea.extent.width;
                regions[idx][aspectRegionIndex].extent.height  = renderArea.extent.height;
                regions[idx][aspectRegionIndex].extent.depth   = 1;
                regions[idx][aspectRegionIndex].numSlices      = sliceCount;
                regions[idx][aspectRegionIndex].swizzledFormat = Pal::UndefinedSwizzledFormat;
            }
        }

        PalCmdResolveImage<true>(
            *srcAttachment.pImage,
            srcLayout,
            *dstAttachment.pImage,
            dstLayout,
            aspectRegionCount,
            regions[0]);
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

    utils::IterateMask deviceGroup(m_palDeviceMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        for (uint32_t i = 0; i < targets.colorTargetCount; ++i)
        {
            const RPAttachmentReference& reference = targets.colorTargets[i];

            if (reference.attachment != VK_ATTACHMENT_UNUSED)
            {
                const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(reference.attachment);

                params.colorTargets[i].pColorTargetView = attachment.pView->PalColorTargetView(deviceIdx);
                params.colorTargets[i].imageLayout = RPGetAttachmentLayout(reference.attachment,
                    Pal::ImageAspect::Color);

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

            const Framebuffer::Attachment& attachment = m_state.allGpuState.pFramebuffer->GetAttachment(attachmentIdx);

            params.depthTarget.pDepthStencilView = attachment.pView->PalDepthStencilView(deviceIdx);
            params.depthTarget.depthLayout       = RPGetAttachmentLayout(attachmentIdx, Pal::ImageAspect::Depth);
            params.depthTarget.stencilLayout     = RPGetAttachmentLayout(attachmentIdx, Pal::ImageAspect::Stencil);

        }
        else
        {
            params.depthTarget.pDepthStencilView = nullptr;
            params.depthTarget.depthLayout       = NullLayout;
            params.depthTarget.stencilLayout     = NullLayout;

        }

        PalCmdBuffer(deviceIdx)->CmdBindTargets(params);

    }
}

// =====================================================================================================================
// Sets view instance mask for a subpass during a render pass instance.
void CmdBuffer::SetViewInstanceMask()
{
    const uint32_t subpassViewMask = m_state.allGpuState.pRenderPass->GetViewMask(m_renderPassInstance.subpass);

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();
        const uint32_t deviceViewMask = uint32_t { 0x1 } << deviceIdx;

        uint32_t viewMask = 0x0;

        if (m_state.allGpuState.ViewIndexFromDeviceIndex)
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
            VK_ASSERT(m_palDeviceMask == viewMask);

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
        const RPExecuteEndRenderPassInfo& end = m_state.allGpuState.pRenderPass->GetExecuteInfo()->end;

        // Synchronize any prior work before leaving the instance (external dependencies) and also handle final layout
        // transitions.
        if (end.flags.hasEndSyncPoint)
        {
            VirtualStackFrame virtStack(m_pStackAllocator);

            RPSyncPoint(end.syncEnd, &virtStack);
        }
    }

    // Clean up instance state
    m_state.allGpuState.pRenderPass   = nullptr;
    m_state.allGpuState.pFramebuffer  = nullptr;
    m_renderPassInstance.pExecuteInfo = nullptr;

    DbgBarrierPostCmd(DbgBarrierEndRenderPass);
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

    if (stageFlags & VK_SHADER_STAGE_COMPUTE_BIT)
    {
        uint32_t bindingPoint = static_cast<uint32_t>(Pal::PipelineBindPoint::Compute);
        PipelineBindState* pBindState = &m_state.allGpuState.pipelineState[bindingPoint];

        Pal::uint32* pUserData = reinterpret_cast<Pal::uint32*>(&pBindState->pushConstData[0]);
        uint32_t* pUserDataPtr = pUserData + startInDwords;

        for (uint32_t i = 0; i < lengthInDwords; i++)
        {
            pUserDataPtr[i] = pInputValues[i];
        }

        pBindState->pushedConstCount = Util::Max(pBindState->pushedConstCount, startInDwords + lengthInDwords);

        // We need access to the user data layout, but avoid dereferencing the pipeline layout if we can help it.
        const PipelineLayout::UserDataLayout& userDataLayout =
            (pLayout == pBindState->pLayout) ? pBindState->userDataLayout : pLayout->GetInfo().userDataLayout;

        // Program the user data register only if the current user data layout base matches that of the given
        // layout.  Otherwise, what's happening is that the application is pushing constants for a future
        // pipeline layout (e.g. at the top of the command buffer) and this register write will be redundant because
        // a future vkCmdBindPipeline will reprogram the user data registers during the rebase.
        if (pBindState->userDataLayout.pushConstRegBase == userDataLayout.pushConstRegBase)
        {
            utils::IterateMask deviceGroup(m_palDeviceMask);
            while (deviceGroup.Iterate())
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdSetUserData(
                    Pal::PipelineBindPoint::Compute,
                    pBindState->userDataLayout.pushConstRegBase + startInDwords,
                    lengthInDwords,
                    pUserDataPtr);
            }
        }
    }

    stageFlags &= ~VK_SHADER_STAGE_COMPUTE_BIT;

    if (stageFlags != 0)
    {
        uint32_t bindingPoint = static_cast<uint32_t>(Pal::PipelineBindPoint::Graphics);
        PipelineBindState* pBindState = &m_state.allGpuState.pipelineState[bindingPoint];

        Pal::uint32* pUserData = reinterpret_cast<Pal::uint32*>(&pBindState->pushConstData[0]);
        uint32_t* pUserDataPtr = pUserData + startInDwords;

        for (uint32_t i = 0; i < lengthInDwords; i++)
        {
            pUserDataPtr[i] = pInputValues[i];
        }

        pBindState->pushedConstCount = Util::Max(pBindState->pushedConstCount, startInDwords + lengthInDwords);

        // We need access to the user data layout, but avoid dereferencing the pipeline layout if we can help it.
        const PipelineLayout::UserDataLayout& userDataLayout =
            (pLayout == pBindState->pLayout) ? pBindState->userDataLayout : pLayout->GetInfo().userDataLayout;

        // Program the user data register only if the current user data layout base matches that of the given
        // layout.  Otherwise, what's happening is that the application is pushing constants for a future
        // pipeline layout (e.g. at the top of the command buffer) and this register write will be redundant because
        // a future vkCmdBindPipeline will reprogram the user data registers during the rebase.
        if (pBindState->userDataLayout.pushConstRegBase == userDataLayout.pushConstRegBase)
        {
            utils::IterateMask deviceGroup(m_palDeviceMask);
            while (deviceGroup.Iterate())
            {
                const uint32_t deviceIdx = deviceGroup.Index();

                PalCmdBuffer(deviceIdx)->CmdSetUserData(
                    Pal::PipelineBindPoint::Graphics,
                    pBindState->userDataLayout.pushConstRegBase + startInDwords,
                    lengthInDwords,
                    pUserDataPtr);
            }
        }
    }

    DbgBarrierPostCmd(DbgBarrierBindSetsPushConstants);
}

// =====================================================================================================================
void CmdBuffer::RequestRenderPassEvents(
    uint32_t     eventCount,
    GpuEvents*** pppGpuEvents)
{
    VK_ASSERT(m_pGpuEventMgr != nullptr);

    // This function may fail if we've run out of system/video memory.  There is no way to return "out of memory"
    // during command buffer building -- the function is just expected to succeed.  Under these extreme conditions,
    // the render pass logic will fall back to using a hard pipeline barrier between every node.
    VkResult result = m_pGpuEventMgr->RequestEvents(this, eventCount, pppGpuEvents);

    if (result != VK_SUCCESS)
    {
        // This situation should be so rare that it's worth asserting here.  If we actually ever hit this condition,
        // we are probably leaking GPU memory somewhere.
        VK_ALERT("Failed to create GPU events for render passes.");

        *pppGpuEvents = nullptr;
    }
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

    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const bool khrMaintenance1 = ((m_pDevice->VkPhysicalDevice()->GetEnabledAPIVersion() >= VK_MAKE_VERSION(1, 1, 0)) ||
                                  m_pDevice->IsExtensionEnabled(DeviceExtensions::KHR_MAINTENANCE1));

    for (uint32_t i = 0; i < viewportCount; ++i)
    {
        VkToPalViewport(pViewports[i], firstViewport + i, khrMaintenance1, &m_state.allGpuState.viewport);
    }

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetViewports(m_state.allGpuState.viewport);
    }

    m_state.allGpuState.staticTokens.viewports = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetAllViewports(
    const Pal::ViewportParams& params,
    uint32_t                   staticToken)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    m_state.allGpuState.viewport.count = params.count;

    for (uint32_t i = 0; i < params.count; ++i)
    {
        m_state.allGpuState.viewport.viewports[i] = params.viewports[i];
    }

    VK_ASSERT(m_palDeviceUsedMask == m_pDevice->GetPalDeviceMask());
    utils::IterateMask deviceGroup(m_palDeviceUsedMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdSetViewports(m_state.allGpuState.viewport);
    }

    m_state.allGpuState.staticTokens.viewports = staticToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetScissor(
    uint32_t            firstScissor,
    uint32_t            scissorCount,
    const VkRect2D*     pScissors)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    for (uint32_t i = 0; i < scissorCount; ++i)
    {
        VkToPalScissorRect(pScissors[i], firstScissor + i, &m_state.allGpuState.scissor);
    }

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetScissorRects(m_state.allGpuState.scissor);
    }

    m_state.allGpuState.staticTokens.scissorRect = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetAllScissors(
    const Pal::ScissorRectParams& params,
    uint32_t                      staticToken)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    m_state.allGpuState.scissor.count = params.count;

    for (uint32_t i = 0; i < params.count; ++i)
    {
        m_state.allGpuState.scissor.scissors[i] = params.scissors[i];
    }

    VK_ASSERT(m_palDeviceUsedMask == m_pDevice->GetPalDeviceMask());
    utils::IterateMask deviceGroup(m_palDeviceUsedMask);
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdSetScissorRects(m_state.allGpuState.scissor);
    }

    m_state.allGpuState.staticTokens.scissorRect = staticToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetLineWidth(
    float               lineWidth)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    constexpr float PointWidth           = 1.0f;    // gl_PointSize is arbitrary, elsewhere pointSize is 1.0
    const VkPhysicalDeviceLimits& limits = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetLimits();

    const Pal::PointLineRasterStateParams params = { PointWidth,
                                                     lineWidth,
                                                     limits.pointSizeRange[0],
                                                     limits.pointSizeRange[1] };

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetPointLineRasterState(params);
    }

    m_state.allGpuState.staticTokens.pointLineRasterState = DynamicRenderStateToken;

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

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetDepthBiasState(params);
    }

    m_state.allGpuState.staticTokens.depthBiasState = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetBlendConstants(
    const float         blendConst[4])
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const Pal::BlendConstParams params = { blendConst[0], blendConst[1], blendConst[2], blendConst[3] };

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetBlendConst(params);
    }

    m_state.allGpuState.staticTokens.blendConst = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetDepthBounds(
    float               minDepthBounds,
    float               maxDepthBounds)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    const Pal::DepthBoundsParams params = { minDepthBounds, maxDepthBounds };

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        PalCmdBuffer(deviceGroup.Index())->CmdSetDepthBounds(params);
    }

    m_state.allGpuState.staticTokens.depthBounds = DynamicRenderStateToken;

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetStencilCompareMask(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilCompareMask)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::FrontReadMask, static_cast<uint8_t>(stencilCompareMask));
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::BackReadMask, static_cast<uint8_t>(stencilCompareMask));
    }
    // Flush the stencil setting, knowing a subsequent vkCmdSetStencilxxx call will also write its own PM4 packets
    // It is done this way to avoid draw-time validation

    m_stencilCombiner.PalCmdSetStencilState(this);
    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetStencilWriteMask(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilWriteMask)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::FrontWriteMask, static_cast<uint8_t>(stencilWriteMask));
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::BackWriteMask, static_cast<uint8_t>(stencilWriteMask));
    }

    // Flush the stencil setting, knowing a subsequent vkCmdSetStencilxxx call will also write its own PM4 packets
    // It is done this way to avoid draw-time validation
    m_stencilCombiner.PalCmdSetStencilState(this);

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
void CmdBuffer::SetStencilReference(
    VkStencilFaceFlags  faceMask,
    uint32_t            stencilReference)
{
    DbgBarrierPreCmd(DbgBarrierSetDynamicPipelineState);

    if (faceMask & VK_STENCIL_FACE_FRONT_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::FrontRef, static_cast<uint8_t>(stencilReference));
    }
    if (faceMask & VK_STENCIL_FACE_BACK_BIT)
    {
        m_stencilCombiner.Set(StencilRefMaskParams::BackRef, static_cast<uint8_t>(stencilReference));
    }

    // Flush the stencil setting, knowing a subsequent vkCmdSetStencilxxx call will also write its own PM4 packets
    // It is done this way to avoid draw-time validation
    m_stencilCombiner.PalCmdSetStencilState(this);

    DbgBarrierPostCmd(DbgBarrierSetDynamicPipelineState);
}

// =====================================================================================================================
// Get a safe number of objects that can be allocated by the virtual stack frame allocator without risking OOM error.
uint32_t CmdBuffer::EstimateMaxObjectsOnVirtualStack(size_t objectSize) const
{
    // Return at least 1 and use only 50% of the remaining space.
    return 1 + static_cast<uint32_t>((m_pStackAllocator->Remaining() / objectSize) >> 1);
}

#if VK_ENABLE_DEBUG_BARRIERS
// =====================================================================================================================
// This function inserts a command before or after a particular Vulkan command if the given runtime settings are asking
// for it.
void CmdBuffer::DbgCmdBarrier(bool preCmd)
{
    const RuntimeSettings& settings = m_pDevice->GetRuntimeSettings();

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

    PalCmdBarrier(barrier);
}
#endif

// =====================================================================================================================
void CmdBuffer::WriteBufferMarker(
    VkPipelineStageFlagBits pipelineStage,
    VkBuffer                dstBuffer,
    VkDeviceSize            dstOffset,
    uint32_t                marker)
{
    const Buffer* pDestBuffer        = Buffer::ObjectFromHandle(dstBuffer);
    const Pal::HwPipePoint pipePoint = VkToPalSrcPipePointForMarkers(pipelineStage, m_palEngineType);

    utils::IterateMask deviceGroup(m_palDeviceMask);

    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        PalCmdBuffer(deviceIdx)->CmdWriteImmediate(
            pipePoint,
            marker,
            Pal::ImmediateDataWidth::ImmediateData32Bit,
            pDestBuffer->GpuVirtAddr(deviceIdx) + dstOffset);
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
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BindDescriptorSets(
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
        pOffsets);
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
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirectCountAMD(
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
VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirectCountAMD(
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
    VK_IGNORE(dependencyFlags);

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
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BeginQuery(queryPool, query, flags);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(
    VkCommandBuffer                             cmdBuffer,
    VkQueryPool                                 queryPool,
    uint32_t                                    query)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->EndQuery(queryPool, query);
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
    VkCommandBuffer                             cmdBuffer,
    const VkRenderPassBeginInfo*                pRenderPassBegin,
    VkSubpassContents                           contents)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->BeginRenderPass(pRenderPassBegin, contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(
    VkCommandBuffer                             cmdBuffer,
    VkSubpassContents                           contents)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->NextSubPass(contents);
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(
    VkCommandBuffer                             cmdBuffer)
{
    ApiCmdBuffer::ObjectFromHandle(cmdBuffer)->EndRenderPass();
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
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBaseKHX(
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
VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMaskKHX(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDeviceMask(deviceMask);
}

#ifdef ICD_VULKAN_1_1
// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDispatchBaseKHR(
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
VKAPI_ATTR void VKAPI_CALL vkCmdSetDeviceMaskKHR(
    VkCommandBuffer                             commandBuffer,
    uint32_t                                    deviceMask)
{
    ApiCmdBuffer::ObjectFromHandle(commandBuffer)->SetDeviceMask(deviceMask);
}
#endif

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
    // The SQTT layer shadows this extension's functions and contains extra code to make use of them.  This
    // extension is not enabled when the SQTT layer is not also enabled, so these functions are currently
    // just blank placeholder functions in case there will be a time where we need to do something with them
    // on this path also.
}

// =====================================================================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT(
    VkCommandBuffer                             commandBuffer)
{
    // The SQTT layer shadows this extension's functions and contains extra code to make use of them.  This
    // extension is not enabled when the SQTT layer is not also enabled, so these functions are currently
    // just blank placeholder functions in case there will be a time where we need to do something with them
    // on this path also.
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

} // namespace entry

} // namespace vk
