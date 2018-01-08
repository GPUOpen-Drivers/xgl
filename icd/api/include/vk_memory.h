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

namespace Pal
{
class IGpuMemory;
}

namespace vk
{
class Device;
class PeerMemory;
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
        priority(static_cast<uint32_t>(level)),
        offset(static_cast<uint32_t>(offset))
    {
    }

    Pal::GpuMemPriority PalPriority() const
        { return static_cast<Pal::GpuMemPriority>(priority); }

    Pal::GpuMemPriorityOffset PalOffset() const
        { return static_cast<Pal::GpuMemPriorityOffset>(offset); }

    bool operator<(const MemoryPriority& priority) const
        { return u32All < priority.u32All; }

    static MemoryPriority FromSetting(uint32_t value);

    struct
    {
        uint32_t priority : 16;
        uint32_t offset   : 16;
    };
    uint32_t u32All;
};

// =====================================================================================================================
// Implementation of a VkMemory object.
class Memory : public NonDispatchable<VkDeviceMemory, Memory>
{
public:
    static VkResult Create(
        Device*                          pDevice,
        const VkMemoryAllocateInfo*      pAllocInfo,
        const VkAllocationCallbacks*     pAllocator,
        VkDeviceMemory*                  pMemory);

    static VkResult OpenExternalMemory(
        Device*                          pDevice,
        const Pal::OsExternalHandle      handle,
        bool                             isNtHandle,
        Memory**                         pMemory);

    Pal::OsExternalHandle GetShareHandle(VkExternalMemoryHandleTypeFlagBitsKHR handleType);

    VkResult Free(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    VkResult Map(
        VkFlags         flags,
        VkDeviceSize    offset,
        VkDeviceSize    size,
        void**          ppData);

    VkResult Unmap(void);

    Pal::Result Init();

    VK_INLINE bool IsMultiInstance() const
    {
        return m_multiInstance;
    }

    VK_INLINE bool IsMirroredAllocation(uint32_t allocationInst) const
    {
        return (m_mirroredAllocationMask & (1 << allocationInst)) != 0;
    }

    VkResult GetCommitment(VkDeviceSize* pCommittedMemoryInBytes);

    VK_INLINE const Pal::GpuMemoryCreateInfo& PalInfo() const { return m_info; }

    void ElevatePriority(MemoryPriority priority);

public:

    VK_INLINE Pal::IGpuMemory* PalMemory(uint32_t deviceIdx = DefaultDeviceIndex) const
    {
        return m_pPalMemory[deviceIdx];
    }

    VK_INLINE PeerMemory* GetPeerMemory() const
    {
        return m_pPeerMemory;
    }

    VK_INLINE Pal::IImage* GetExternalPalImage() const
    {
        return m_pExternalPalImage;
    }

protected:
    Device*                      m_pDevice;
    Pal::IGpuMemory*             m_pPalMemory[MaxPalDevices];
    PeerMemory*                  m_pPeerMemory;
    Pal::GpuMemoryCreateInfo     m_info;
    MemoryPriority               m_priority;
    uint32_t                     m_allocationMask;
    uint32_t                     m_mirroredAllocationMask;
    bool                         m_multiInstance;
private:
    Memory(vk::Device*         pDevice,
           Pal::IGpuMemory**   pPalMemory,
           PeerMemory*         pPeerMemory,
           uint32_t            allocationMask,
           const Pal::GpuMemoryCreateInfo& info,
           Pal::IImage*        pPalExternalImage = nullptr);

    // Image needs to be a friend class to be able to create wrapper API memory objects
    friend class Image;

    bool         m_allocationCounted;
    Pal::IImage* m_pExternalPalImage;

    // the function is used to mark that the allocation is counted in the logical device.
    // the destructor of this memory object need to decrease the count.
    VK_INLINE void SetAllocationCounted() { m_allocationCounted = true; }

    Pal::Result MirrorSharedAllocation();

    // Private constructor used by Image objects to create wrapper API memory object for presentable image
    Memory(Device*           pDevice,
           Pal::IGpuMemory** pPalMemory,
           PeerMemory*       pPeerMemory,
           uint32_t          allocationMask);

    static VkResult OpenExternalSharedImage(
        Device*                      pDevice,
        Image*                       pBoundImage,
        const Pal::OsExternalHandle  handle,
        bool                         isNtHandle,
        Memory**                     ppVkMemory);
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

VKAPI_ATTR void VKAPI_CALL vkGetDeviceMemoryFDAMD(
    VkDevice                                device,
    VkDeviceMemory                          memory,
    int*                                    pFD);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdKHR(
    VkDevice                                device,
    const VkMemoryGetFdInfoKHR*             pGetFdInfo,
    int*                                    pFd);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFdPropertiesKHR(
    VkDevice                                device,
    VkExternalMemoryHandleTypeFlagBitsKHR   handleType,
    int                                     fd,
    VkMemoryFdPropertiesKHR*                pMemoryFdProperties);

} // namespace entry

} // namespace vk

#endif /* __VK_MEMORY_H__ */
