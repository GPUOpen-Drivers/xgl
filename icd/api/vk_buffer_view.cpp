/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_object.h"
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

    // Build the SRD
    Pal::BufferViewInfo info = {};

    Buffer* pBuffer = Buffer::ObjectFromHandle(pCreateInfo->buffer);

    info.swizzledFormat = VkToPalFormat(pCreateInfo->format, pDevice->GetRuntimeSettings());
    info.stride         = Pal::Formats::BytesPerPixel(info.swizzledFormat.format);
    if (pCreateInfo->range == VK_WHOLE_SIZE)
    {
        /* "If range is equal to VK_WHOLE_SIZE, the range from offset to the end of the buffer is used.
         *  If VK_WHOLE_SIZE is used and the remaining size of the buffer is not a multiple of the
         *  element size of format, then the nearest smaller multiple is used." */
        info.range = Util::RoundDownToMultiple(pBuffer->GetSize() - pCreateInfo->offset, info.stride);
    }
    else
    {
        info.range = pCreateInfo->range;
    }

    void* pSrdMemory = Util::VoidPtrInc(pMemory, apiSize);
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        info.gpuAddr = pBuffer->GpuVirtAddr(deviceIdx) + pCreateInfo->offset;

        if (pCreateInfo->format != VK_FORMAT_UNDEFINED)
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

    VK_PLACEMENT_NEW (pMemory) BufferView (pDevice, static_cast<uint32_t>(srdSize), pSrdMemory);

    *pBufferView = BufferView::HandleFromVoidPointer(pMemory);

    return VK_SUCCESS;
}

// ===============================================================================================
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

// ===============================================================================================
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
