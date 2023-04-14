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
 ***********************************************************************************************************************
 * @file  vk_memory.h
 * @brief GPU memory object related functionality for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __vk_MEMORY_H__
#define __vk_MEMORY_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "palGpuMemory.h"

namespace vk
{
class Device;
class Image;
};

namespace vk
{

// =====================================================================================================================
// Helper structure representing a IGpuMemory priority+offset pair.
union MemoryPriority
{
    MemoryPriority() {}
    MemoryPriority(Pal::GpuMemPriority level, Pal::GpuMemPriorityOffset offset)
        :
        offset(static_cast<uint32_t>(offset)),
        priority(static_cast<uint32_t>(level))
    {
    }

    Pal::GpuMemPriority PalPriority() const
        { return static_cast<Pal::GpuMemPriority>(priority); }

    Pal::GpuMemPriorityOffset PalOffset() const
        { return static_cast<Pal::GpuMemPriorityOffset>(offset); }

    bool operator<(const MemoryPriority& memPriority) const
        { return ((priority < memPriority.priority) ||
                  ((priority == memPriority.priority) && (offset < memPriority.offset))); }

    bool operator!=(const MemoryPriority& memPriority) const
        { return ((priority != memPriority.priority) ||
                  ((priority == memPriority.priority) && (offset != memPriority.offset))); }

    static MemoryPriority FromSetting(uint32_t value);

    static MemoryPriority FromVkMemoryPriority(float value);

    struct
    {
        uint32_t offset   : 16;
        uint32_t priority : 16;
    };
    uint32_t u32All;
};

// =====================================================================================================================
// Specifies properties for opening an external shared memory.
struct ImportMemoryInfo
{
    Pal::OsExternalHandle   handle;         // A handle on Windows, or a fd on Linux.
    bool                    isNtHandle;     // It's a Windows-specific flag indicates the handle is shared via NT.
};

// =====================================================================================================================
// Implementation of a VkMemory object.
class Memory final : public NonDispatchable<VkDeviceMemory, Memory>
{
public:
    static VkResult Create(
        Device*                          pDevice,
        const VkMemoryAllocateInfo*      pAllocInfo,
        const VkAllocationCallbacks*     pAllocator,
        VkDeviceMemory*                  pMemory);

    static VkResult OpenExternalMemory(
        Device*                     pDevice,
        const ImportMemoryInfo&     importInfo,
        Memory**                    ppMemory);

    Pal::OsExternalHandle GetShareHandle(VkExternalMemoryHandleTypeFlagBits handleType);

    void Free(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    void Init(
        Pal::IGpuMemory**   pPalMemory);

    VkResult Map(
        VkFlags         flags,
        VkDeviceSize    offset,
        VkDeviceSize    size,
        void**          ppData);

    void Unmap(void);

    bool IsMultiInstance() const
    {
        return m_flags.multiInstance;
    }

    VkResult GetCommitment(VkDeviceSize* pCommittedMemoryInBytes);

    void ElevatePriority(MemoryPriority priority);

    void SetPriority(
        const MemoryPriority    priority,
        const bool              mustBeLower);

    Pal::IGpuMemory* PalMemory(uint32_t resourceIndex, uint32_t memoryIndex);

    Pal::IGpuMemory* PalMemory(uint32_t resourceIndex) const
    {
        return m_pPalMemory[resourceIndex][resourceIndex];
    }

    Pal::IImage* GetExternalPalImage() const
    {
        return m_pExternalPalImage;
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Memory);

    Memory(vk::Device*                      pDevice,
           Pal::IGpuMemory**                pPalMemory,
           Pal::OsExternalHandle            externalHandle,
           const Pal::GpuMemoryCreateInfo&  createInfo,
           bool                             multiInstance     = false,
           uint32_t                         primaryIndex      = DefaultDeviceIndex,
           Pal::IImage*                     pPalExternalImage = nullptr);

    // Image needs to be a friend class to be able to create wrapper API memory objects
    friend class Image;

    // Marks that the logical device's allocated memory and needs to decrease the allocated memory size during the
    // destruction of this memory object.
    void MarkAllocatedMemory(uint32_t sizeAccountedForDeviceMask)
    {
        m_sizeAccountedForDeviceMask = sizeAccountedForDeviceMask;
    }

    // Private constructor used by Image objects to create wrapper API memory object for presentable image
    Memory(Device*           pDevice,
           Pal::IGpuMemory** pPalMemory,
           bool              multiInstance = false,
           uint32_t          primaryIndex  = DefaultDeviceIndex);

    static void GetPrimaryDeviceIndex(
        uint32_t  maxDevices,
        uint32_t  allocationMask,
        uint32_t* pIndex,
        bool*     pMultiInstance);

    static VkResult CreateGpuMemory(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator,
        const Pal::GpuMemoryCreateInfo& createInfo,
        const Pal::GpuMemoryExportInfo& exportInfo,
        uint32_t                        allocationMask,
        bool                            multiInstanceHeap,
        Memory**                        ppMemory);

    static VkResult CreateGpuPinnedMemory(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator,
        const Pal::GpuMemoryCreateInfo& createInfo,
        uint32_t                        allocationMask,
        bool                            multiInstanceHeap,
        bool                            isHostMappedForeign,
        void*                           pPinnedHostPtr,
        Memory**                        ppMemory);

    static VkResult OpenExternalSharedImage(
        Device*                 pDevice,
        Image*                  pBoundImage,
        const ImportMemoryInfo& importInfo,
        Memory**                ppVkMemory);

    Device*               m_pDevice;
    Pal::IGpuMemory*      m_pPalMemory[MaxPalDevices][MaxPalDevices];
    Pal::IImage*          m_pExternalPalImage;

    // Cache the handle of GPU memory which is on the first device, if the Gpumemory can be inter-process sharing.
    Pal::OsExternalHandle m_sharedGpuMemoryHandle;

    Pal::gpusize          m_size;
    Pal::GpuHeap          m_heap0;
    MemoryPriority        m_priority;
    uint32_t              m_sizeAccountedForDeviceMask;
    uint32_t              m_primaryDeviceIndex;

    union
    {
        struct
        {
            uint32_t sharedViaNtHandle :  1;
            uint32_t multiInstance     :  1;
            uint32_t reserved1         :  1;
            uint32_t reserved          : 29;
        };

        uint32_t u32All;
    } m_flags;
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize                                offset,
    VkDeviceSize                                size,
    VkMemoryMapFlags                            flags,
    void**                                      ppData);

VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(
    VkDevice                                    device,
    VkDeviceMemory                              memory);

VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory2KHR(
    VkDevice                                    device,
    const VkMemoryMapInfoKHR*                   pMemoryMapInfo,
    void**                                      ppData);

VKAPI_ATTR VkResult VKAPI_CALL vkUnmapMemory2KHR(
    VkDevice                                    device,
    const VkMemoryUnmapInfoKHR*                 pMemoryUnmapInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges);

VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(
    VkDevice                                    device,
    uint32_t                                    memoryRangeCount,
    const VkMappedMemoryRange*                  pMemoryRanges);

VKAPI_ATTR void VKAPI_CALL vkGetDeviceMemoryCommitment(
    VkDevice                                    device,
    VkDeviceMemory                              memory,
    VkDeviceSize*                               pCommittedMemoryInBytes);

VKAPI_ATTR uint64_t VKAPI_CALL vkGetDeviceMemoryOpaqueCaptureAddress(
    VkDevice                                         device,
    const VkDeviceMemoryOpaqueCaptureAddressInfo*    pInfo);

#if defined(__unix__)
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdKHR(
    VkDevice                                device,
    const VkMemoryGetFdInfoKHR*             pGetFdInfo,
    int*                                    pFd);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
    VkDevice                                device,
    VkExternalMemoryHandleTypeFlagBits      handleType,
    int                                     fd,
    VkMemoryFdPropertiesKHR*                pMemoryFdProperties);
#endif

} // namespace entry

} // namespace vk

#endif /* __VK_MEMORY_H__ */
