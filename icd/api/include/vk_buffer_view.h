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

#ifndef __VK_BUFFER_VIEW_H__
#define __VK_BUFFER_VIEW_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "include/vk_buffer.h"

namespace vk
{
// Forward declare Vulkan classes used in this file
class Device;
class ApiBufferView;

class BufferView final : public NonDispatchable<VkBufferView, BufferView>
{
public:
    static VkResult Create(
        Device*                           pDevice,
        const VkBufferViewCreateInfo*     pCreateInfo,
        const VkAllocationCallbacks*      pAllocator,
        VkBufferView*                     pBufferView);

    static void BuildSrd(
        const Device*                     pDevice,
        const VkDeviceSize                bufferOffset,
        const VkDeviceSize                bufferRange,
        const Pal::gpusize*               bufferAddress,
        const VkFormat                    format,
        const uint32_t                    deviceNum,
        const size_t                      srdSize,
        void*                             pSrdMemory);

    VkResult Destroy(
        Device*                           pDevice,
        const VkAllocationCallbacks*      pAllocator);

    const void* Descriptor(VkDescriptorType descType, int32_t deviceIdx) const
    {
        return Util::VoidPtrInc(m_pSrds, m_SrdSize * deviceIdx);
    }

protected:
    BufferView(
        Device*     pDevice,
        uint32_t    srdSize,
        const void* pSrds);

    const Device* const     m_pDevice;
    uint32_t                m_SrdSize;   // size of the Srd in bytes
    const void*             m_pSrds;     // Pointer to the SRD of the buffer views

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(BufferView);
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyBufferView(
    VkDevice                                    device,
    VkBufferView                                bufferView,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_BUFFER_VIEW_H__ */
