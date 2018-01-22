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

#ifndef __VK_BUFFER_H__
#define __VK_BUFFER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"

namespace Pal
{

class IGpuMemory;

}

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;
class Memory;
class PeerMemory;

class Buffer : public NonDispatchable<VkBuffer, Buffer>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkBufferCreateInfo*       pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkBuffer*                       pBuffer);

    VK_FORCEINLINE Pal::gpusize GpuVirtAddr(int32_t idx = DefaultDeviceIndex) const
       { return m_gpuVirtAddr[idx]; }

    VK_FORCEINLINE Pal::IGpuMemory* PalMemory(uint32_t idx = DefaultDeviceIndex) const
       { return m_pGpuMemory[idx]; }

    Memory* GetMemoryObj() const
       { return m_pMemory; }

    VK_FORCEINLINE uint8_t GetMemoryInstanceIdx(uint32_t idx) const
       { return m_multiInstanceIndices[idx]; }

    VK_FORCEINLINE VkDeviceSize MemOffset() const
       { return m_memOffset; }

    VkResult Destroy(
        const Device*                   pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult BindMemory(
        VkDeviceMemory     mem,
        VkDeviceSize       memOffset,
        const uint32_t*    pDeviceIndices);

    VkResult GetMemoryRequirements(
        VkMemoryRequirements* pMemoryRequirements);

    VkDeviceSize GetSize() const
        { return m_size; }

    uint32_t GetSupportedOutputCoherMask() const
        { return m_outputCacheMask; }

    uint32_t GetSupportedInputCoherMask() const
        { return m_inputCacheMask; }

    // We have to treat the buffer sparse if any of these flags are set
    static const VkBufferCreateFlags SparseEnablingFlags =
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT |
        VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;

    bool IsSparse() const
        { return (m_flags & SparseEnablingFlags) != 0; }

    bool DedicatedMemoryRequired() const { return m_internalFlags.dedicatedRequired; }

private:

    union BufferFlags
    {
        struct
        {
            uint32_t internalMemBound    : 1;   // If this buffer has an internal memory bound, the bound memory
                                                // should be destroyed when this buffer is destroyed.
            uint32_t dedicatedRequired   : 1;   // Indicates the allocation of buffer is dedicated.
            uint32_t externallyShareable : 1;   // True if the backing memory of this buffer may be shared externally.
            uint32_t reserved            : 29;
        };
        uint32_t     u32All;
    };

    Buffer(Device*                      pDevice,
           VkBufferCreateFlags          flags,
           VkBufferUsageFlags           usage,
           Pal::IGpuMemory**            pGpuMemory,
           VkDeviceSize                 size,
           BufferFlags                  internalFlags);

    void CalcBarrierUsage(
        const Device*      pDevice,
        VkBufferUsageFlags usage);

    Pal::IGpuMemory*        m_pGpuMemory[MaxPalDevices];
    Pal::gpusize            m_gpuVirtAddr[MaxPalDevices];
    uint8_t                 m_multiInstanceIndices[MaxPalDevices];
    Memory*                 m_pMemory;

    const VkDeviceSize      m_size;
    VkDeviceSize            m_memOffset;
    Device* const           m_pDevice;
    VkBufferCreateFlags     m_flags;
    VkBufferUsageFlags      m_usage;
    uint32_t                m_outputCacheMask;  // This is the maximum set of supported output caches this buffer can
                                                // touch.
    uint32_t                m_inputCacheMask;   // This is the maximum set of supported input caches this buffer can
                                                // touch.
    BufferFlags             m_internalFlags;    // Flags describing the properties of this buffer
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkDeviceMemory                              memory,
    VkDeviceSize                                memoryOffset);

VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice                                    device,
    VkBuffer                                    buffer,
    VkMemoryRequirements*                       pMemoryRequirements);

VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2KHR(
    VkDevice                                    device,
    const VkBufferMemoryRequirementsInfo2KHR*   pInfo,
    VkMemoryRequirements2KHR*                   pMemoryRequirements);

} // namespace entry

} // namespace vk

#endif /* __VK_BUFFER_H__ */
