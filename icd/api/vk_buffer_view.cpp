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

#include "include/vk_buffer_view.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"

#include "palFormatInfo.h"

namespace vk
{

// =====================================================================================================================
// Create a new Vulkan Buffer View object
VkResult BufferView::Create(
    Device*                            pDevice,
    const VkBufferViewCreateInfo*      pCreateInfo,
    const VkAllocationCallbacks*       pAllocator,
    VkBufferView*                      pBufferView)
{
    VK_ASSERT(pCreateInfo->sType == VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);

    // Allocate memory for the buffer view
    const size_t apiSize = sizeof(BufferView);
    const size_t bufferSrdSize =
        pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties().gfxipProperties.srdSizes.bufferView;
    size_t srdSize = bufferSrdSize;

    const size_t objSize = apiSize +
        (srdSize * pDevice->NumPalDevices());

    void* pMemory = pDevice->AllocApiObject(pAllocator, objSize);

    if (pMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    void* pSrdMemory = Util::VoidPtrInc(pMemory, apiSize);

    Buffer*      pBuffer                      = Buffer::ObjectFromHandle(pCreateInfo->buffer);
    Pal::gpusize bufferAddress[MaxPalDevices] = {};

    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        bufferAddress[deviceIdx] = pBuffer->GpuVirtAddr(deviceIdx);
    }

    VkDeviceSize range = pCreateInfo->range;

    if (range == VK_WHOLE_SIZE)
    {
        const RuntimeSettings&    settings       = pDevice->GetRuntimeSettings();
        const Pal::SwizzledFormat swizzledFormat = VkToPalFormat(pCreateInfo->format, settings);
        const Pal::gpusize        stride         = Pal::Formats::BytesPerPixel(swizzledFormat.format);

        range = Util::RoundDownToMultiple(pBuffer->GetSize() - pCreateInfo->offset, stride);
    }

    BuildSrd(pDevice,
             pCreateInfo->offset,
             range,
             bufferAddress,
             pCreateInfo->format,
             pDevice->NumPalDevices(),
             srdSize,
             pSrdMemory);

    VK_PLACEMENT_NEW(pMemory) BufferView(pDevice, static_cast<uint32_t>(srdSize), pSrdMemory);

    *pBufferView = BufferView::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// =====================================================================================================================
void BufferView::BuildSrd(
    const Device*                 pDevice,
    const VkDeviceSize            bufferOffset,
    const VkDeviceSize            bufferRange,
    const Pal::gpusize*           bufferAddress,
    const VkFormat                format,
    const uint32_t                deviceNum,
    const size_t                  srdSize,
    void*                         pSrdMemory)
{
    // Build the SRD
    Pal::BufferViewInfo info        = {};
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();

    info.swizzledFormat = VkToPalFormat(format, settings);
    info.stride         = Pal::Formats::BytesPerPixel(info.swizzledFormat.format);
    info.range          = bufferRange;

    // Bypass Mall read/write if no alloc policy is set for SRDs
    if (Util::TestAnyFlagSet(settings.mallNoAllocResourcePolicy, MallNoAllocBufferViewSrds))
    {
        info.flags.bypassMallRead = 1;
        info.flags.bypassMallWrite = 1;
    }

    for (uint32_t deviceIdx = 0; deviceIdx < deviceNum; deviceIdx++)
    {
        info.gpuAddr = bufferAddress[deviceIdx] + bufferOffset;

        if (format != VK_FORMAT_UNDEFINED)
        {
            pDevice->PalDevice(deviceIdx)->CreateTypedBufferViewSrds(
                1, &info, Util::VoidPtrInc(pSrdMemory, srdSize * deviceIdx));
        }
        else
        {
            info.stride = 0; // Raw buffers have a zero byte stride

            pDevice->PalDevice(deviceIdx)->CreateUntypedBufferViewSrds(
                1, &info, Util::VoidPtrInc(pSrdMemory, srdSize * deviceIdx));
        }

        VK_ASSERT(srdSize >=
                  pDevice->VkPhysicalDevice(deviceIdx)->PalProperties().gfxipProperties.srdSizes.bufferView);
    }
}

// =====================================================================================================================
BufferView::BufferView(
    Device*             pDevice,
    uint32_t            srdSize,
    const void*         pSrds)
    :
    m_pDevice(pDevice),
    m_SrdSize(srdSize),
    m_pSrds(pSrds)
{
}

// =====================================================================================================================
// Destroy a buffer object
VkResult BufferView::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    Util::Destructor(this);

    pDevice->FreeApiObject(pAllocator, this);

    return VK_SUCCESS;
}

namespace entry
{

// =====================================================================================================================

VKAPI_ATTR void VKAPI_CALL vkDestroyBufferView(
    VkDevice                                    device,
    VkBufferView                                bufferView,
    const VkAllocationCallbacks*                pAllocator)
{
    if (bufferView != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        BufferView::ObjectFromHandle(bufferView)->Destroy(pDevice, pAllocCB);
    }
}

} // namespace entry

} // namespace vk
