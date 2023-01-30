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

#ifndef __VK_BUFFER_H__
#define __VK_BUFFER_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"

#include "include/barrier_policy.h"

namespace Pal
{

class IGpuMemory;

}

namespace vk
{

// Forward declare Vulkan classes used in this file
class Device;
class Memory;

class Buffer final : public NonDispatchable<VkBuffer, Buffer>
{
public:
    static VkResult Create(
        Device*                         pDevice,
        const VkBufferCreateInfo*       pCreateInfo,
        const VkAllocationCallbacks*    pAllocator,
        VkBuffer*                       pBuffer);

    VK_FORCEINLINE Pal::gpusize GpuVirtAddr(int32_t idx) const
       { return m_perGpu[idx].gpuVirtAddr; }

    VK_FORCEINLINE Pal::IGpuMemory* PalMemory(uint32_t idx) const
       { return m_perGpu[idx].pGpuMemory; }

    VK_FORCEINLINE VkDeviceSize MemOffset() const
       { return m_memOffset; }

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult BindMemory(
        const Device*      pDevice,
        VkDeviceMemory     mem,
        VkDeviceSize       memOffset,
        const uint32_t*    pDeviceIndices);

    void GetMemoryRequirements(
        const Device*         pDevice,
        VkMemoryRequirements* pMemoryRequirements);

    static void CalculateMemoryRequirements(
        const Device*                              pDevice,
        const VkDeviceBufferMemoryRequirementsKHR* pInfo,
        VkMemoryRequirements2*                     pMemoryRequirements);

    VkDeviceSize GetSize() const
        { return m_size; }

    // We have to treat the buffer sparse if any of these flags are set
    static const VkBufferCreateFlags SparseEnablingFlags =
        VK_BUFFER_CREATE_SPARSE_BINDING_BIT |
        VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;

    bool IsSparse() const
        { return (m_internalFlags.createSparseBinding | m_internalFlags.createSparseResidency) != 0; }

    bool DedicatedMemoryRequired() const { return m_internalFlags.dedicatedRequired; }

    VK_FORCEINLINE const BufferBarrierPolicy& GetBarrierPolicy() const
        { return m_barrierPolicy; }

private:

    PAL_DISALLOW_COPY_AND_ASSIGN(Buffer);

    struct BufferExtStructs
    {
        const VkExternalMemoryBufferCreateInfo* pExternalMemoryBufferCreateInfo;
    };

    union BufferFlags
    {
        struct
        {
            uint32_t internalMemBound      : 1;   // If this buffer has an internal memory bound, the bound memory
                                                  // should be destroyed when this buffer is destroyed.
            uint32_t dedicatedRequired     : 1;   // Indicates the allocation of buffer is dedicated.
            uint32_t externallyShareable   : 1;   // True if the backing memory of this buffer may be shared externally.
            uint32_t externalPinnedHost    : 1;   // True if backing memory for this buffer may be imported from a pinned
                                                  // host allocation.
            uint32_t usageUniformBuffer    : 1;   // VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
#if VKI_RAY_TRACING
            uint32_t usageAccelStorage     : 1;   // VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
#else
            uint32_t reserved1             : 1;
#endif
            uint32_t createSparseBinding   : 1;   // VK_BUFFER_CREATE_SPARSE_BINDING_BIT
            uint32_t createSparseResidency : 1;   // VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT
            uint32_t createProtected       : 1;   // VK_BUFFER_CREATE_PROTECTED_BIT
            uint32_t reserved2             : 1;
            uint32_t usageDescriptor       : 1;   // VK_BUFFER_USAGE_DESCRIPTOR_BUFFER_BIT_EXT
            uint32_t reserved              : 21;
        };
        uint32_t     u32All;
    };

    struct PerGpuInfo
    {
        Pal::IGpuMemory*    pGpuMemory;
        Pal::gpusize        gpuVirtAddr;
    };

    Buffer(Device*                      pDevice,
           const VkBufferCreateInfo*    pCreateInfo,
           Pal::IGpuMemory**            pGpuMemory,
           BufferFlags                  internalFlags);

    // Compute size required for the object.  One copy of PerGpuInfo is included in the object and we need
    // to add space for any additional GPUs.
    static size_t ObjectSize(const Device* pDevice)
    {
        return sizeof(Buffer) + ((pDevice->NumPalDevices() - 1) * sizeof(PerGpuInfo));
    }

    static void LogBufferCreate(
        const VkBufferCreateInfo* pCreateInfo,
        VkBuffer                  buffer,
        const Device*             pDevice);

    void LogGpuMemoryBind(const Device* pDevice, const Pal::IGpuMemory* pPalMemory, VkDeviceSize memOffset) const;

    static void GetBufferMemoryRequirements(
        const Device*         pDevice,
        const BufferFlags*    pBufferFlags,
        const VkDeviceSize    size,
        VkMemoryRequirements* pMemoryRequirements);

    static void CalculateBufferFlags(
        const Device*             pDevice,
        const VkBufferCreateInfo* pCreateInfo,
        const BufferExtStructs&   pExtStructs,
        BufferFlags*              pBufferFlags);

    static void HandleExtensionStructs(
        const VkBufferCreateInfo*   pCreateInfo,
        BufferExtStructs*           pExtStructs);

    const VkDeviceSize      m_size;
    VkDeviceSize            m_memOffset;
    BufferBarrierPolicy     m_barrierPolicy;     // Barrier policy to use for this buffer
    BufferFlags             m_internalFlags;     // Flags describing the properties of this buffer

    // This goes last.  The memory for the rest of the array is calculated dynamically based on the number of GPUs in
    // use.
    PerGpuInfo              m_perGpu[1];
};

// =====================================================================================================================
inline Pal::gpusize GetBufferAddress(
    uint32_t     deviceIndex,
    VkBuffer     buffer,
    VkDeviceSize offset)
{
    if (buffer != VK_NULL_HANDLE)
    {
        return Buffer::ObjectFromHandle(buffer)->GpuVirtAddr(deviceIndex) + Pal::gpusize(offset);
    }
    else
    {
        return 0;
    }
}

// =====================================================================================================================
inline Pal::gpusize GetBufferSize(
    VkBuffer     buffer,
    VkDeviceSize offset)
{
    if (buffer != VK_NULL_HANDLE)
    {
        return Buffer::ObjectFromHandle(buffer)->GetSize() - Pal::gpusize(offset);
    }
    else
    {
        return 0;
    }
}

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

VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements2(
    VkDevice                                    device,
    const VkBufferMemoryRequirementsInfo2*      pInfo,
    VkMemoryRequirements2*                      pMemoryRequirements);

VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddress(
    VkDevice                                    device,
    const VkBufferDeviceAddressInfo*            pInfo);

VKAPI_ATTR uint64_t VKAPI_CALL vkGetBufferOpaqueCaptureAddress(
    VkDevice                                    device,
    const VkBufferDeviceAddressInfo*            pInfo);

} // namespace entry

} // namespace vk

#endif /* __VK_BUFFER_H__ */
