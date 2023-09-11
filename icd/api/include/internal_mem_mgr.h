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
 **************************************************************************************************
 * @file  internal_mem_mgr.h
 * @brief Internal memory manager class declaration.
 **************************************************************************************************
 */

#ifndef __INTERNAL_MEM_MGR_H__
#define __INTERNAL_MEM_MGR_H__

#pragma once

#include "include/khronos/vk_platform.h"
#include "include/vk_alloccb.h"
#include "include/vk_defines.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"

#include "palList.h"
#include "palHashMap.h"
#include "palHashSet.h"
#include "palMutex.h"
#include "palBuddyAllocator.h"
#include "palGpuMemory.h"
#include "palGpuMemoryBindable.h"
#include "palDevice.h"

namespace vk
{

// Forward declarations
class Device;
class Instance;
class InternalMemMgr;

// Flags for describing internal memory allocations.
union InternalMemCreateFlags
{
    struct
    {
        uint32_t readOnly         : 1;  // This is a GPU read-only allocation.
        uint32_t persistentMapped : 1;  // Persistently map this GPU allocation.  This flag should be set for
                                        // frequently mapped allocations.
        uint32_t needShadow       : 1;  // If a shadow table is needed.
        uint32_t needGl2Uncached  : 1;  // If a gl2Uncached is needed.
        uint32_t debug            : 1;  // Memory used for internal debugging (e.g. data dumping) only;
                                        // not to be mixed with regular sub-allocations
        uint32_t reserved         : 27; // Reserved
    };
    uint32_t u32All;
};

// Structure for describing internal memory allocations
struct InternalMemCreateInfo
{
    Pal::GpuMemoryCreateInfo pal; // PAL GPU memory create info
    InternalMemCreateFlags flags; // Creation flags
    void*              pPoolInfo; // Return value from a previous call to CalcSubAllocationPool() that can be used
                                  // to accelerate sub-allocation if it is known that a future sub-allocation can come
                                  // from the same kind of pool.  This parameter is optional and can be nullptr.
};

// Structure holding information about the properties of internal GPU memory base allocations that identifies a
// memory pool suitable for a particular use
struct MemoryPoolProperties
{
    InternalMemCreateFlags flags;                    // Create flags governing this pool
    Pal::VaRange           vaRange;                  // Virtual address range to use
    uint32_t               heapCount;                // Number of heaps in the heap preference array
    Pal::GpuHeap           heaps[Pal::GpuHeapCount]; // Heap preference array
};

// =====================================================================================================================
// Device Group Memory struct, a container for memory and access for multi-gpu
struct DeviceGroupMemory
{
    Pal::IGpuMemory*    pPalMemory[MaxPalDevices];          // PAL GPU memory object of the internal base allocation
    void*               pPersistentCpuAddr[MaxPalDevices];  // Persistently mapped CPU address
};

// Structure holding information about an internal GPU memory base allocation
struct InternalMemoryPool
{
    DeviceGroupMemory                   groupMemory;        // Memory allocations for each physical device contained
                                                            // within a single logical device
                                                            // TODO. Match VA addresses across devices where available

    DeviceGroupMemory                   groupShadowMemory;  // Memory allocations for the shadow memory.

    Util::BuddyAllocator<PalAllocator>* pBuddyAllocator; // Buddy allocator used to sub-allocate
                                                         // from the pool
};

// =====================================================================================================================
// Internal memory class responsible to hold information about an internal memory suballocation
class InternalMemory
{
public:
    inline InternalMemory();

    Pal::IGpuMemory* PalMemory(int32_t idx)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_memoryPool.groupMemory.pPalMemory[idx];
    }

    Pal::IGpuMemory* PalMemory(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_memoryPool.groupMemory.pPalMemory[idx];
    }

    Pal::gpusize GpuVirtAddr(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_gpuVA[idx];
    }

    Pal::gpusize GpuShadowVirtAddr(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_gpuShadowVA[idx];
    }

    void* CpuAddr(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return Util::VoidPtrInc(m_memoryPool.groupMemory.pPersistentCpuAddr[idx], static_cast<size_t>(m_offset));
    }

    Pal::gpusize Offset() const
        { return m_offset; }

    Pal::gpusize Size() const
        { return m_size; }

    Pal::Result Map(uint32_t idx, void** pCpuAddr);
    Pal::Result ShadowMap(uint32_t idx, void** pCpuAddr);

    Pal::Result Unmap(uint32_t idx);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(InternalMemory);

    friend class InternalMemMgr;

    InternalMemoryPool  m_memoryPool;                   // Memory pool the suballocation comes from (its pBuddyAllocator is
                                                        // null if the memory is base allocation, not a suballocation)
    Pal::gpusize        m_gpuVA[MaxPalDevices];         // GPU virtual address to the start of the sub-allocation
    Pal::gpusize        m_gpuShadowVA[MaxPalDevices];   // GPU virtual address for the shadow table
    Pal::gpusize        m_offset;                       // Offset within the memory pool the suballocation starts from
    Pal::gpusize        m_size;                         // Size of the suballocation
    Pal::gpusize        m_alignment;                    // Alignment of the suballocation
};

// =====================================================================================================================
InternalMemory::InternalMemory()
    :
    m_offset(0),
    m_size(0),
    m_alignment(0)
{
    memset(&m_gpuVA,       0, sizeof(m_gpuVA));
    memset(&m_gpuShadowVA, 0, sizeof(m_gpuShadowVA));
    memset(&m_memoryPool,  0, sizeof(m_memoryPool));
}

// These are identifiers for commonly used pool configurations for internal memory allocation that can be used
// through InternalMemMgr::GetCommonPool() instead of calling CalcSubAllocationPool()
enum InternalSubAllocPool
{
    InternalPoolGpuReadOnlyRemote      = 0, // All read-only persistent mapped CPU-visible pools in system memory
    InternalPoolGpuReadOnlyCpuVisible,      // All read-only persistent mapped CPU-visible pools (incl. local visible)
    InternalPoolCpuVisible,                 // All CPU-visible pools
    InternalPoolGpuAccess,                  // All GPU-visible pools
    InternalPoolDescriptorTable,            // Persistent mapped pool used for descriptor sets (main table)
    InternalPoolCpuCacheableGpuUncached,    // Cacheable CPU-visible pool with enabled gl2Uncached if requred for the GPU
    InternalPoolDebugGpuAccess,             // GPU-only memory meant for debugging infrastructure
    InternalPoolDebugCpuRead,               // CPU-only (GART cacheable) memory meant for debugging infrastructure
    InternalPoolCount
};

// =====================================================================================================================
// Internal memory manager class responsible for managing GPU memory allocations needed for internal purposes by the
// Vulkan API layer.
class InternalMemMgr
{
public:
    InternalMemMgr(Device*   pDevice,
                   Instance* pInstance);
    ~InternalMemMgr() { Destroy(); }

    VkResult Init();
    void Destroy();

    VkResult AllocGpuMem(
        const InternalMemCreateInfo& internalInfo,
        InternalMemory*              pInternalMemory,
        uint32_t                     allocMask,
        const VkObjectType           requestingObjectType,
        const uint64_t               requestingObjectHandle);

    VkResult AllocAndBindGpuMem(
        uint32_t                   numDevices,
        Pal::IGpuMemoryBindable**  ppBindableObjectPerDevice,
        bool                       readOnly,
        InternalMemory*            pInternalMemory,
        uint32_t                   allocMask,
        bool                       removeInvisibleHeap,
        bool                       persistentMapped,
        const VkObjectType         requestingObjectType,
        const uint64_t             requestingObjectHandle);

    void FreeGpuMem(
        const InternalMemory*           pInternalMemory);

    void GetCommonPool(InternalSubAllocPool poolId, InternalMemCreateInfo* pAllocInfo) const;

    VkResult CalcSubAllocationPool(const MemoryPoolProperties& poolProps, void** ppPoolInfo);

    void  DestroyDeviceGroupMemory(const struct DeviceGroupMemory* pGroupMemory, Instance* pInstance);

    Pal::Result Map(struct DeviceGroupMemory* pGroupMemory);
    Pal::Result Unmap(const struct DeviceGroupMemory* pGroupMemory);

    void GetVirtualAddress(struct DeviceGroupMemory* pGroupMemory, Pal::gpusize* pGpuVA, Pal::gpusize memOffset);

private:
    typedef Util::List<InternalMemoryPool, PalAllocator>                                               MemoryPoolList;
    typedef Util::HashMap<MemoryPoolProperties, MemoryPoolList*, PalAllocator, Util::JenkinsHashFunc>  MemoryPoolListMap;

    PAL_DISALLOW_COPY_AND_ASSIGN(InternalMemMgr);

    VkResult CalcSubAllocationPoolInternal(
        const MemoryPoolProperties& poolProps,
        MemoryPoolList**            ppPoolInfo);

    void CheckProvidedSubAllocPoolInfo(const InternalMemCreateInfo& memInfo) const;

    VkResult CreateMemoryPoolList(
        const MemoryPoolProperties& poolProps,
        MemoryPoolList**            ppNewList);

    VkResult CreateMemoryPoolAndSubAllocate(
        MemoryPoolList*              pOwnerList,
        const InternalMemCreateInfo& initialSubAllocInfo,
        InternalMemory*              pMemory,
        uint32_t                     allocMask);

    VkResult AllocBaseGpuMem(
        const Pal::GpuMemoryCreateInfo& createInfo,
        const InternalMemCreateFlags&   memCreateFlags,
        InternalMemoryPool*             pGpuMemory,
        uint32_t                        allocMask,
        const bool                      needShadow,
        const bool                      isBuddyAllocated);

    void FreeBaseGpuMem(
        const InternalMemoryPool*       pGpuMemory);

    void FilterViableHeaps(
        const Pal::GpuHeap* pHeaps,
        uint32_t            heapCount,
        Pal::GpuHeap*       pOutHeaps,
        uint32_t*           pOutHeapCount);

    Device* const       m_pDevice;          // Logical device this memory manager belongs to

    Pal::GpuMemoryHeapProperties m_heapProps[Pal::GpuHeapCount]; // Information about the memory heaps

    PalAllocator*       m_pSysMemAllocator; // Allocator object for system-memory allocations
    Util::Mutex         m_allocatorLock;    // Serialize access to the memory manager to ensure thread-safety
    MemoryPoolListMap   m_poolListMap;      // Maintain a hash map of memory pool lists for each property combination

    MemoryPoolProperties m_commonPoolProps[InternalPoolCount]; // Commonly used pool properties
    void*                m_pCommonPools[InternalPoolCount];    // Commonly used memory pools
};

// =====================================================================================================================
// Helper class for mapping vram in an RAII fashion
class MappedInternalMemoryAddr
{
public:
    // Default Constructor: Valid (safely destructable) but unusable state.
    MappedInternalMemoryAddr() noexcept {}

    // Destructor: Only destroy the pointer we mapped. Assume m_pVidMem is externally managed.
    ~MappedInternalMemoryAddr() noexcept
    {
        if (IsMapped() &&
            Util::IsErrorResult(m_pVidMem->Unmap(m_deviceIdx)))
        {
            VK_ALERT_ALWAYS_MSG("Unable to unmap CPU address");
        }
    }

    // Attempts to map video memory. On failure the object remains safely descrutable
    void Init(InternalMemory* pMappableMem, uint32_t deviceIdx) noexcept
    {
        VK_ASSERT(pMappableMem != nullptr);

        if (Util::IsErrorResult(pMappableMem->Map(deviceIdx, &m_pCpuAddr)) == false)
        {
            m_pVidMem   = pMappableMem;
            m_deviceIdx = deviceIdx;
        }
        else
        {
            VK_ALERT_ALWAYS_MSG("Unable to map InternalMemory to CPU address");
        }
    }

    // Accessors to the mapped memory
    bool     IsMapped() const noexcept { return m_pCpuAddr != nullptr; }
    void*    Get()      const noexcept { return m_pCpuAddr; }
    uint64_t Size()     const noexcept { return this->IsMapped() ? m_pVidMem->Size() : 0; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(MappedInternalMemoryAddr);

    void*           m_pCpuAddr  = nullptr;
    InternalMemory* m_pVidMem   = nullptr;
    uint32_t        m_deviceIdx = 0;
};

} // namespace vk

#endif /* __INTERNAL_MEM_MGR_H__ */
