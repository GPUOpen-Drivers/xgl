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
/**
 **************************************************************************************************
 * @file  internal_mem_mgr.cpp
 * @brief Internal memory manager class implementation.
 **************************************************************************************************
 */

#include "include/internal_mem_mgr.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_instance.h"
#include "include/vk_physical_device.h"
#include "include/vk_utils.h"

#include "palInlineFuncs.h"
#include "palBuddyAllocatorImpl.h"
#include "palListImpl.h"
#include "palHashMapImpl.h"
#include "palHashSetImpl.h"

namespace vk
{

static constexpr Pal::gpusize PoolAllocationSize        = 1ull << 18;   // 256 kilobytes
static constexpr Pal::gpusize PoolMinSuballocationSize  = 1ull << 4;    // 16 bytes

// =====================================================================================================================
// Filter invisible heap. For some objects as pipeline, invisible heap will be appended in memory requirement.
// We filter this because we don't expect to support object memory migration.
static VK_INLINE void FilterHeap(
    Pal::GpuMemoryRequirements* pMemReq,
    Pal::GpuHeap typeToFilter)
{
    uint32_t origHeapCount = pMemReq->heapCount;

    pMemReq->heapCount = 0;

    for (uint32_t i = 0; i< origHeapCount; ++i)
    {
        if (pMemReq->heaps[i] != typeToFilter)
        {
            pMemReq->heaps[pMemReq->heapCount++] = pMemReq->heaps[i];
        }
    }
}

// =====================================================================================================================
InternalMemMgr::InternalMemMgr(
    Device*   pDevice,
    Instance* pInstance)
    :
    m_pDevice(pDevice),
    m_pSysMemAllocator(pInstance->Allocator()),
    m_poolListMap(32, m_pSysMemAllocator)
{
    memset(m_commonPoolProps, 0, sizeof(m_commonPoolProps));
    memset(m_pCommonPools, 0, sizeof(m_pCommonPools));
}

// =====================================================================================================================
// Initializes the internal memory manager.
VkResult InternalMemMgr::Init()
{
    VkResult result = VK_SUCCESS;

    // Initialize memory manager lock
    Pal::Result palResult = m_allocatorLock.Init();

    if (palResult == Pal::Result::Success)
    {
        // Initialize pool list map
        palResult = m_poolListMap.Init();
    }

    if (palResult == Pal::Result::Success)
    {
        // Get heap specific information
        palResult = m_pDevice->PalDevice(DefaultDeviceIndex)->GetGpuMemoryHeapProperties(m_heapProps);
    }

    result = PalToVkResult(palResult);

    // Precompute commonly used pool information
    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].flags.readOnly         = true;
        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].flags.persistentMapped = true;

        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].vaRange                = Pal::VaRange::Default;

        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].heapCount              = 2;
        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].heaps[0]               = Pal::GpuHeapGartUswc;
        m_commonPoolProps[InternalPoolGpuReadOnlyRemote].heaps[1]               = Pal::GpuHeapGartCacheable;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolGpuReadOnlyRemote],
            &m_pCommonPools[InternalPoolGpuReadOnlyRemote]);
    }

    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].flags.readOnly         = true;
        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].flags.persistentMapped = true;

        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].vaRange                = Pal::VaRange::Default;

        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].heapCount              = 3;
        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].heaps[0]               = Pal::GpuHeapLocal;
        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].heaps[1]               = Pal::GpuHeapGartCacheable;
        m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible].heaps[2]               = Pal::GpuHeapGartUswc;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible],
            &m_pCommonPools[InternalPoolGpuReadOnlyCpuVisible]);
    }

    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolCpuVisible].flags.persistentMapped = true;

        m_commonPoolProps[InternalPoolCpuVisible].vaRange   = Pal::VaRange::Default;

        m_commonPoolProps[InternalPoolCpuVisible].heapCount = 3;
        m_commonPoolProps[InternalPoolCpuVisible].heaps[0]  = Pal::GpuHeapLocal;
        m_commonPoolProps[InternalPoolCpuVisible].heaps[1]  = Pal::GpuHeapGartUswc;
        m_commonPoolProps[InternalPoolCpuVisible].heaps[2]  = Pal::GpuHeapGartCacheable;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolCpuVisible],
            &m_pCommonPools[InternalPoolCpuVisible]);
    }

    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolGpuAccess].vaRange = Pal::VaRange::Default;

        m_commonPoolProps[InternalPoolGpuAccess].heapCount = 4;
        m_commonPoolProps[InternalPoolGpuAccess].heaps[0] = Pal::GpuHeapInvisible;
        m_commonPoolProps[InternalPoolGpuAccess].heaps[1] = Pal::GpuHeapLocal;
        m_commonPoolProps[InternalPoolGpuAccess].heaps[2] = Pal::GpuHeapGartUswc;
        m_commonPoolProps[InternalPoolGpuAccess].heaps[3] = Pal::GpuHeapGartCacheable;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolGpuAccess],
            &m_pCommonPools[InternalPoolGpuAccess]);
    }

    if (result == VK_SUCCESS)
    {
        // For descriptor tables use a GPU-read-only CPU-visible pool with a corresponding VA range.
        // This ensures that the top 32 bits of descriptor table addresses will be a known value to SC thus
        // it's enough to provide a 32-bit descriptor set address with the lower 32 bits through user data.
        m_commonPoolProps[InternalPoolDescriptorTable] = m_commonPoolProps[InternalPoolGpuReadOnlyCpuVisible];
        m_commonPoolProps[InternalPoolDescriptorTable].vaRange = Pal::VaRange::DescriptorTable;

        // Set the shadow flag for descriptor table
        m_commonPoolProps[InternalPoolDescriptorTable].flags.needShadow =
            m_pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolDescriptorTable],
            &m_pCommonPools[InternalPoolDescriptorTable]);
    }

    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolCpuCacheableGpuUncached].flags.persistentMapped = true;
        m_commonPoolProps[InternalPoolCpuCacheableGpuUncached].flags.needGl2Uncached = 1;

        m_commonPoolProps[InternalPoolCpuCacheableGpuUncached].vaRange = Pal::VaRange::Default;

        m_commonPoolProps[InternalPoolCpuCacheableGpuUncached].heapCount = 1;
        m_commonPoolProps[InternalPoolCpuCacheableGpuUncached].heaps[0] = Pal::GpuHeapGartCacheable;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolCpuCacheableGpuUncached],
            &m_pCommonPools[InternalPoolCpuCacheableGpuUncached]);
    }

    if (result == VK_SUCCESS)
    {
        m_commonPoolProps[InternalPoolDebugDump] = m_commonPoolProps[InternalPoolCpuCacheableGpuUncached];

        m_commonPoolProps[InternalPoolDebugDump].flags.debug = 1;

        result = CalcSubAllocationPool(
            m_commonPoolProps[InternalPoolDebugDump],
            &m_pCommonPools[InternalPoolDebugDump]);
    }

    return result;
}

// =====================================================================================================================
// Populates the heap allocation and sub-allocation pool information for a particular upcoming memory allocation
// based on a commonly used internal pool configuration
void InternalMemMgr::GetCommonPool(
    InternalSubAllocPool   poolId,
    InternalMemCreateInfo* pAllocInfo
    ) const
{
    pAllocInfo->pPoolInfo             = m_pCommonPools[poolId];
    pAllocInfo->flags.u32All          = m_commonPoolProps[poolId].flags.u32All;
    pAllocInfo->pal.vaRange           = m_commonPoolProps[poolId].vaRange;
    pAllocInfo->pal.heapCount         = static_cast<uint32_t>(m_commonPoolProps[poolId].heapCount);

    memcpy(pAllocInfo->pal.heaps, m_commonPoolProps[poolId].heaps, sizeof(pAllocInfo->pal.heaps));
}

// =====================================================================================================================
// Tears down the internal memory manager.
void InternalMemMgr::Destroy()
{
    // Delete the suballocators (the GPU memory objects corresponding to them is already deleted)
    while (m_poolListMap.GetNumEntries() != 0)
    {
        auto mapIt = m_poolListMap.Begin();

        MemoryPoolList* pPoolList = mapIt.Get()->value;

        while (pPoolList->NumElements() != 0)
        {
            auto it = pPoolList->Begin();

            InternalMemoryPool* pPool = it.Get();

            pPool->groupMemory.Unmap();

            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                m_pDevice->RemoveMemReference(m_pDevice->PalDevice(deviceIdx),
                                              pPool->groupMemory.PalMemory(deviceIdx));
            }

            // Delete the memory object and the system memory associated with it
            pPool->groupMemory.Destroy(m_pDevice->VkInstance());

            // Delete Shadow Memory
            pPool->groupShadowMemory.Unmap();

            for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
            {
                if (pPool->groupShadowMemory.PalMemory(deviceIdx) != nullptr)
                {
                    m_pDevice->RemoveMemReference(m_pDevice->PalDevice(deviceIdx),
                                                    pPool->groupShadowMemory.PalMemory(deviceIdx));
                }
            }

            pPool->groupShadowMemory.Destroy(m_pDevice->VkInstance());

            // Delete the buddy allocator
            PAL_DELETE(pPool->pBuddyAllocator, m_pSysMemAllocator);

            // Remove item from list
            pPoolList->Erase(&it);
        }

        // Free this list
        PAL_DELETE(pPoolList, m_pSysMemAllocator);

        // Erase item from the hash map
        m_poolListMap.Erase(mapIt.Get()->key);
    }

}

// =====================================================================================================================
// This function computes compatible memory pool properties from a particular sub-allocation's create info.
static void GetMemoryPoolPropertiesFromAllocInfo(
    const InternalMemCreateInfo& memInfo,
    MemoryPoolProperties*        pPoolProps)
{
    pPoolProps->flags     = memInfo.flags;
    pPoolProps->vaRange   = memInfo.pal.vaRange;
    pPoolProps->heapCount = memInfo.pal.heapCount;

    for (uint32_t h = 0; h < memInfo.pal.heapCount; ++h)
    {
        pPoolProps->heaps[h] = memInfo.pal.heaps[h];
    }
}

// =====================================================================================================================
// This function can be called by memory manager clients to pre-compute which pool future sub-allocations come from
// so long as those future sub-allocations' properties match the given memory pool properties to this function.
VkResult InternalMemMgr::CalcSubAllocationPool(
    const MemoryPoolProperties& poolProps,
    void**                      ppPoolInfo)
{
    Util::MutexAuto lock(&m_allocatorLock); // Ensure thread-safety using the lock

    return CalcSubAllocationPoolInternal(poolProps, reinterpret_cast<MemoryPoolList**>(ppPoolInfo));
}

// =====================================================================================================================
// Internal version of CalcSubAllocationPool() that doesn't take the lock.
//
// WARNING: This function is NOT thread-safe and assumes the caller is holding a lock on m_allocatorLock.
VkResult InternalMemMgr::CalcSubAllocationPoolInternal(
    const MemoryPoolProperties& poolProps,
     MemoryPoolList**           ppPoolList)
{
#if DEBUG
    // If persistent mapping is requested, make sure only CPU-visible heaps are enabled
    for (uint32_t h = 0; h < poolProps.heapCount; ++h)
    {
        VK_ASSERT(!poolProps.flags.persistentMapped || m_heapProps[poolProps.heaps[h]].flags.cpuVisible);
    }
#endif

    VkResult result = VK_SUCCESS;

    // Find a previously-seen memory pool list corresponding to the requested memory pool properties.
    MemoryPoolList** ppExistingList = m_poolListMap.FindKey(poolProps);

    // If one already exists, return that; if no memory pool list exists yet for the requested memory pool properties
    // then create a new one.
    if (ppExistingList != nullptr)
    {
        *ppPoolList = *ppExistingList;
    }
    else
    {
        result = CreateMemoryPoolList(poolProps, ppPoolList);

        if (result != VK_SUCCESS)
        {
            *ppPoolList = nullptr;
        }
    }

    return result;
}

// =====================================================================================================================
// This function creates a new memory pool list.  This list describes a set of large allocations that have
// homogenous properties in terms of GPU heap, etc.  Sub-allocations will be made by looking for space within the
// entries in this list.
//
// WARNING: This function is NOT thread-safe and assumes the caller is holding a lock on m_allocatorLock.
VkResult InternalMemMgr::CreateMemoryPoolList(
    const MemoryPoolProperties& poolProps,
    MemoryPoolList**            ppNewList)
{
    VkResult result = VK_SUCCESS;

    MemoryPoolList* pPoolList = PAL_NEW(MemoryPoolList, m_pSysMemAllocator, Util::AllocInternal) (m_pSysMemAllocator);

    if (pPoolList != nullptr)
    {
        // Add this pool list to the pool list map
        Pal::Result palResult = m_poolListMap.Insert(poolProps, pPoolList);

        if (palResult != Pal::Result::Success)
        {
            // On failure release the system memory allocated and set the appropriate error code
            PAL_DELETE(pPoolList, m_pSysMemAllocator);

            pPoolList = nullptr;

            result = PalToVkResult(palResult);
        }
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    *ppNewList = pPoolList;

    return result;
}

// =====================================================================================================================
// Creates a new MemoryPool allocation and inserts it into the given list that can be used for future sub-allocation.
// An initial sub-allocation will be made from the pool and information for that sub-allocation will be returned by this
// function.
//
// WARNING: This function is NOT thread-safe and assumes the caller is holding a lock on m_allocatorLock.
VkResult InternalMemMgr::CreateMemoryPoolAndSubAllocate(
    MemoryPoolList*              pOwnerList,
    const InternalMemCreateInfo& initialSubAllocInfo,
    InternalMemoryPool*          pNewPool,
    uint32_t                     allocMask,
    Pal::gpusize*                pSubAllocOffset)
{
    InternalMemCreateInfo poolInfo = initialSubAllocInfo;

    // Use a larger, fixed size for pool allocations so that future sub-allocations will succeed
    poolInfo.pal.size = Util::Pow2Align(PoolAllocationSize, poolInfo.pal.alignment);

    VK_ASSERT(poolInfo.pal.size >= PoolMinSuballocationSize);
    VK_ASSERT(poolInfo.pal.size >= initialSubAllocInfo.pal.size);

    InternalMemoryPool newPool  = {};
    Pal::gpusize subAllocOffset = 0;

    // Allocate the base GPU memory object for this pool
    VkResult result = VK_SUCCESS;

    // If the GPU memory is allocated then create a buddy allocator for it
    newPool.pBuddyAllocator = PAL_NEW(Util::BuddyAllocator<PalAllocator>, m_pSysMemAllocator, Util::AllocInternal)
        (m_pSysMemAllocator, poolInfo.pal.size, PoolMinSuballocationSize);

    if (newPool.pBuddyAllocator != nullptr)
    {
        // If the buddy allocator was successfully created then initialize it
        Pal::Result palResult = newPool.pBuddyAllocator->Init();

        result = PalToVkResult(palResult);
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Suballocate from the newly-created pool
    if (result == VK_SUCCESS)
    {
        // NOTE: The suballocation should never fail here since we just obtained a fresh base
        // allocation, the only possible case for failure is a low system memory situation
        Pal::Result palResult = newPool.pBuddyAllocator->Allocate(
            initialSubAllocInfo.pal.size,
            initialSubAllocInfo.pal.alignment,
            &subAllocOffset);

        result = PalToVkResult(palResult);
    }

    // If we succeeded, return the new sub-allocation information; otherwise, clean up

    InternalMemoryPool* pInternalMemory = nullptr;

    if (result == VK_SUCCESS)
    {
        Pal::Result palResult = pOwnerList->PushFront(newPool);
        result = PalToVkResult(palResult);
        VK_ASSERT(result == VK_SUCCESS);

        pInternalMemory = pOwnerList->Begin().Get();

        // Allocate the base GPU memory object for this pool
        result = AllocBaseGpuMem(poolInfo.pal,
                                 poolInfo.flags,
                                 pInternalMemory,
                                 allocMask,
                                 initialSubAllocInfo.flags.needShadow);
    }

    // Persistently map the base allocation if requested.
    if ((result == VK_SUCCESS) && (poolInfo.flags.persistentMapped))
    {
        Pal::Result palResult = pInternalMemory->groupMemory.Map();
        result = PalToVkResult(palResult);
    }

    if (result == VK_SUCCESS)
    {
        *pNewPool        = *pInternalMemory;
        *pSubAllocOffset = subAllocOffset;
    }
    else
    {
        auto it = pOwnerList->Begin();
        bool needEraseFromOwnerList = pOwnerList->NumElements() > 0 ?
            (it.Get()->groupMemory.PalMemory(DefaultDeviceIndex) ==
             pInternalMemory->groupMemory.PalMemory(DefaultDeviceIndex)) : false;

        // Unmap any persistently mapped memory
        pInternalMemory->groupMemory.Unmap();

        // Destroy the buddy allocator
        if (pInternalMemory->pBuddyAllocator != nullptr)
        {
            PAL_DELETE(pInternalMemory->pBuddyAllocator, m_pSysMemAllocator);
        }

        // Release this pool's base allocation
        FreeBaseGpuMem(pInternalMemory);

        // Remove this memory pool from the list if we added it
        if (needEraseFromOwnerList)
        {
            pOwnerList->Erase(&it);
        }
    }

    return result;
}

// =====================================================================================================================
// Given information from an internal sub-allocation that has previously called CalcSubAllocationPool() to choose a
// compatible pool for sub-allocation, this function verifies that that sub-allocation's other parameters are still
// consistent with that chosen pool.  E.g. to make sure that the heaps chosen for this sub-allocation still match those
// used to previously compute the pool.
void InternalMemMgr::CheckProvidedSubAllocPoolInfo(
    const InternalMemCreateInfo& memInfo
    ) const
{
#if DEBUG
    VK_ASSERT(memInfo.pPoolInfo != nullptr);

    MemoryPoolProperties poolProps = {};

    GetMemoryPoolPropertiesFromAllocInfo(memInfo, &poolProps);

    MemoryPoolList** ppExistingList = m_poolListMap.FindKey(poolProps);

    VK_ASSERT((ppExistingList != nullptr) && (*ppExistingList == memInfo.pPoolInfo));
#endif
}

// =====================================================================================================================
// Allocates GPU memory for internal use. Depending on the type of memory object requested, the memory may be
// sub-allocated from an existing allocation, or it might not.
// Any new allocations are added to the residency list automatically.
VkResult InternalMemMgr::AllocGpuMem(
    const InternalMemCreateInfo& createInfo,
    InternalMemory*              pInternalMemory,
    uint32_t                     allocMask)
{
    VK_ASSERT(pInternalMemory != nullptr);

    Util::MutexAuto lock(&m_allocatorLock); // Ensure thread-safety using the lock

    VkResult result = VK_SUCCESS;

    // If the requested allocation is small enough (at most half the size of a single pool) then try to find an
    // appropriate pool and suballocate from it.
    if ((createInfo.flags.noSuballocation == false) &&
        (createInfo.pal.size <= (PoolAllocationSize / 2)))
    {
        MemoryPoolList* pPoolList;

        // Use the previously computed pool list if one is provided.  Otherwise choose one based on this
        // sub-allocation's information.
        if (createInfo.pPoolInfo != nullptr)
        {
#if DEBUG
            CheckProvidedSubAllocPoolInfo(createInfo);
#endif
            pPoolList = reinterpret_cast<MemoryPoolList*>(createInfo.pPoolInfo);
        }
        else
        {
            // No previously-computed pool has been provided so find one for this allocation
            MemoryPoolProperties poolProps = {};

            GetMemoryPoolPropertiesFromAllocInfo(createInfo, &poolProps);

            result = CalcSubAllocationPoolInternal(poolProps, &pPoolList);
        }

        if (result == VK_SUCCESS)
        {
            // Assume that we won't find an appropriate pool
            result = VK_ERROR_OUT_OF_DEVICE_MEMORY;

            // If found an appropriate pool list then search for a memory pool to suballocate from
            for (auto it = pPoolList->Begin(); it.Get() != nullptr; it.Next())
            {
                InternalMemoryPool* pPool = it.Get();

                // Try to suballocate from the current memory pool using its buddy allocator
                Pal::Result palResult = pPool->pBuddyAllocator->Allocate(
                    createInfo.pal.size,
                    createInfo.pal.alignment,
                    &pInternalMemory->m_offset);

                if (palResult == Pal::Result::Success)
                {
                    // If the suballocation succeeded, set the memory pool the suballocation came from
                    pInternalMemory->m_memoryPool = *pPool;

                    // Set the result to success and quit the loop
                    result = VK_SUCCESS;
                    break;
                }
            }

            if (result != VK_SUCCESS)
            {
                // If at this point we still didn't manage to find an appropriate pool that has enough space then
                // it means we need to create a new memory pool and sub-allocate from that
                result = CreateMemoryPoolAndSubAllocate(
                    pPoolList,
                    createInfo,
                    &pInternalMemory->m_memoryPool,
                    allocMask,
                    &pInternalMemory->m_offset);
            }
        }
    }
    else
    {
        // We don't suballocate from a pool so there's no buddy allocator and also offset is always zero
        pInternalMemory->m_memoryPool.pBuddyAllocator    = nullptr;
        pInternalMemory->m_offset = 0;

        // Issue a base memory allocation and use that as the memory object
        result = AllocBaseGpuMem(
            createInfo.pal,
            createInfo.flags,
            &pInternalMemory->m_memoryPool,
            allocMask,
            createInfo.flags.needShadow);

        // Persistently map the allocation if necessary
        if ((result == VK_SUCCESS) && (createInfo.flags.persistentMapped))
        {
            pInternalMemory->m_memoryPool.groupMemory.Map();

            if (createInfo.flags.needShadow)
            {
                pInternalMemory->m_memoryPool.groupShadowMemory.Map();
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        // On success make sure to fill in the size and alignment information of the allocation
        // This helps making the deallocation faster because we can find the proper kval in the buddy allocator
        // immediately
        pInternalMemory->m_memoryPool.groupMemory.GetVirtualAddress(
                                                    pInternalMemory->m_gpuVA, pInternalMemory->m_offset);

        if (createInfo.flags.needShadow)
        {
            pInternalMemory->m_memoryPool.groupShadowMemory.GetVirtualAddress(
                                                    pInternalMemory->m_gpuShadowVA, pInternalMemory->m_offset);

            // Check the lower part of the VA for the Descriptor and Shadow Table are equal
            VK_ASSERT(static_cast<int32_t>(pInternalMemory->m_gpuVA[0]) ==
                      static_cast<int32_t>(pInternalMemory->m_gpuShadowVA[0]));
        }

        pInternalMemory->m_size      = createInfo.pal.size;
        pInternalMemory->m_alignment = createInfo.pal.alignment;
    }

    return result;
}

// =====================================================================================================================
// Queries the provided GPU-memory-bindable object for its memory requirements, and allocates GPU memory to satisfy
// those requirements. Finally, the memory is bound to the provided object if allocation is successful.
VkResult InternalMemMgr::AllocAndBindGpuMem(
    uint32_t                  numDevices,
    Pal::IGpuMemoryBindable** ppBindableObjectPerDevice,
    bool                      readOnly,
    InternalMemory*           pInternalMemory,
    uint32_t                  allocMask,
    bool                      removeInvisibleHeap,
    bool                      persistentMapped)
{
    VK_ASSERT(pInternalMemory != nullptr);

    // Get the memory requirements of the GPU-memory-bindable object
    Pal::GpuMemoryRequirements memReqs = {};
    ppBindableObjectPerDevice[DefaultDeviceIndex]->GetGpuMemoryRequirements(&memReqs);

    // If the object reports that it doesn't need any GPU memory, return early.
    if (memReqs.heapCount == 0)
    {
        return VK_SUCCESS;
    }

    // Fill in the GPU memory object creation info based on the memory requirements
    InternalMemCreateInfo createInfo = {};

    if (removeInvisibleHeap)
    {
        FilterHeap(&memReqs, Pal::GpuHeap::GpuHeapInvisible);
    }

    createInfo.pal.size               = memReqs.size;
    createInfo.pal.alignment          = memReqs.alignment;
    createInfo.pal.vaRange            = Pal::VaRange::Default;
    createInfo.pal.priority           = Pal::GpuMemPriority::Normal;
    createInfo.flags.readOnly         = readOnly;
    createInfo.flags.persistentMapped = persistentMapped ? 1 : 0;

    const bool sharedAllocation = (numDevices > 1) && (Util::CountSetBits(allocMask) == 1);
    if (sharedAllocation == true)
    {
        createInfo.pal.flags.shareable = 1;

        FilterHeap(&memReqs, Pal::GpuHeap::GpuHeapLocal);
        if (removeInvisibleHeap == false)
        {
            FilterHeap(&memReqs, Pal::GpuHeap::GpuHeapInvisible);
        }
    }

    createInfo.pal.heapCount = memReqs.heapCount;
    for (uint32_t h = 0; h < memReqs.heapCount; ++h)
    {
        createInfo.pal.heaps[h] = memReqs.heaps[h];
    }

    // Issue the memory allocation
    VkResult result = AllocGpuMem(createInfo, pInternalMemory, allocMask);

    if (result == VK_SUCCESS)
    {
        Pal::Result palResult = Pal::Result::Success;

        for (uint32_t deviceIdx = 0; (deviceIdx < numDevices) && (palResult == Pal::Result::Success); deviceIdx++)
        {
            // If the memory allocation succeeded then try to bind the memory to the object
            palResult = ppBindableObjectPerDevice[deviceIdx]->BindGpuMemory(
                pInternalMemory->m_memoryPool.groupMemory.PalMemory(deviceIdx),
                pInternalMemory->m_offset);
        }

        if (palResult != Pal::Result::Success)
        {
            // If binding the memory failed then free the allocated memory and return the error code
            FreeGpuMem(pInternalMemory);
            result = PalToVkResult(palResult);
        }
    }

    return result;
}

// =====================================================================================================================
// Frees GPU memory which was previously allocated for internal use.
void InternalMemMgr::FreeGpuMem(
    const InternalMemory* pInternalMemory)
{
    Util::MutexAuto lock(&m_allocatorLock);

    VK_ASSERT(pInternalMemory != nullptr);

    if (pInternalMemory->m_memoryPool.pBuddyAllocator != nullptr)
    {
        // The memory was suballocated so free it using the buddy allocator
        pInternalMemory->m_memoryPool.pBuddyAllocator->Free(
            pInternalMemory->m_offset,
            pInternalMemory->m_size,
            pInternalMemory->m_alignment);
    }
    else
    {
        VK_ASSERT(pInternalMemory->m_offset == 0);  // Offset should be zero if this isn't a suballocation

        // Unmap if the allocation was persistently mapped.  Note that we only do this here for allocations that
        // are not sub-allocated.
        pInternalMemory->m_memoryPool.groupMemory.Unmap();

        // Free the base allocation
        FreeBaseGpuMem(&pInternalMemory->m_memoryPool);
    }
}

// =====================================================================================================================
// Allocates a base GPU memory object allocation.
VkResult InternalMemMgr::AllocBaseGpuMem(
    const Pal::GpuMemoryCreateInfo& createInfo,
    const InternalMemCreateFlags&   memCreateFlags,
    InternalMemoryPool*             pGpuMemory,
    uint32_t                        allocMask,
    bool                            needShadow)
{
    VK_ASSERT(pGpuMemory != nullptr);

    size_t      palMemSize = 0;
    Pal::Result palResult  = Pal::Result::ErrorOutOfGpuMemory;

    Pal::GpuMemoryCreateInfo     localCreateInfo = createInfo;
    const Pal::DeviceProperties& palProperties   = m_pDevice->VkPhysicalDevice(DefaultDeviceIndex)->PalProperties();

    if (palProperties.gfxipProperties.flags.supportGl2Uncached && memCreateFlags.needGl2Uncached)
    {
        localCreateInfo.flags.gl2Uncached = 1;
    }
    else
    {
        localCreateInfo.flags.gl2Uncached = 0;
    }

    // Query system memory requirement of PAL GPU memory object
    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        palMemSize += m_pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(localCreateInfo, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);
    }

    // Allocate system memory for the object
    void* pSystemMem = m_pDevice->VkInstance()->AllocMem(
        palMemSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

    if (pSystemMem != nullptr)
    {
        size_t   palMemOffset = 0;

        // Issue the memory allocation
        Pal::IGpuMemory* pFirstAlloc = nullptr;

        // Pass 0 - Allocates the memory for each device according to any set bits in allocMask
        // Pass 1 - Shares the allocated memory with other phsyical devices
        const uint32_t numPasses = m_pDevice->NumPalDevices() == Util::CountSetBits(allocMask) ? 1 : 2;

        for (uint32_t passIdx = 0; passIdx < numPasses; passIdx++)
        {
            const bool allocatingMemory = (passIdx == 0);
            const bool mirroringMemory = (passIdx == 1);

            const uint32_t mask = allocatingMemory ? allocMask : ~allocMask;

            for (uint32_t deviceIdx = 0;
                 (deviceIdx < m_pDevice->NumPalDevices()) && (palResult == Pal::Result::Success);
                 deviceIdx++)
            {
                if ((mask & (1 << deviceIdx)) != 0)
                {
                    if (allocatingMemory)
                    {
                        palResult = m_pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
                            localCreateInfo,
                            Util::VoidPtrInc(pSystemMem, palMemOffset),
                            &pGpuMemory->groupMemory.m_pPalMemory[deviceIdx]);

                        if (pFirstAlloc == nullptr)
                        {
                            pFirstAlloc = pGpuMemory->groupMemory.m_pPalMemory[deviceIdx];
                        }

                        if ((palResult == Pal::Result::Success) && needShadow)
                        {
                            // Allocate system memory for the object
                            void* pSystemShadowMem = m_pDevice->VkInstance()->AllocMem(
                                palMemSize,
                                VK_DEFAULT_MEM_ALIGN,
                                VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

                            // Allocate Shadow
                            Pal::gpusize gpuVA[MaxPalDevices];
                            pGpuMemory->groupMemory.GetVirtualAddress(gpuVA, 0);

                            // For shadow descriptor tables use a GPU-read-only CPU-visible pool with a corresponding
                            // VA range. This ensures that the top 32 bits of shadow descriptor table addresses will
                            // be a known value to SC thus it's enough to provide a 32-bit descriptor set address with
                            // the lower 32 bits through user data.
                            Pal::GpuMemoryCreateInfo shadowCreateInfo = localCreateInfo;
                            shadowCreateInfo.descrVirtAddr            = gpuVA[deviceIdx];
                            shadowCreateInfo.vaRange                  = Pal::VaRange::ShadowDescriptorTable;
                            shadowCreateInfo.heapCount                = 1;
                            shadowCreateInfo.heaps[0]                 = Pal::GpuHeapGartCacheable;

                            palResult = m_pDevice->PalDevice(deviceIdx)->CreateGpuMemory(
                                shadowCreateInfo,
                                Util::VoidPtrInc(pSystemShadowMem, palMemOffset),
                                &pGpuMemory->groupShadowMemory.m_pPalMemory[deviceIdx]);
                        }
                    }

                    if (mirroringMemory)
                    {
                        Pal::GpuMemoryOpenInfo shareMem = {};
                        shareMem.pSharedMem = pFirstAlloc;

                        Pal::IDevice* pPalDevice = m_pDevice->PalDevice(deviceIdx);
                        palResult = pPalDevice->OpenSharedGpuMemory(
                            shareMem,
                            Util::VoidPtrInc(pSystemMem, palMemOffset),
                            &pGpuMemory->groupMemory.m_pPalMemory[deviceIdx]);
                    }

                    if (palResult == Pal::Result::Success)
                    {
                        palMemOffset += m_pDevice->PalDevice(deviceIdx)->GetGpuMemorySize(localCreateInfo, &palResult);
                        VK_ASSERT(palResult == Pal::Result::Success);

                        // Add the newly created memory object to the residency list
                        palResult = m_pDevice->AddMemReference(
                            m_pDevice->PalDevice(deviceIdx),
                            pGpuMemory->groupMemory.m_pPalMemory[deviceIdx],
                            memCreateFlags.readOnly);

                        if ((palResult == Pal::Result::Success) && needShadow)
                        {
                            palResult = m_pDevice->AddMemReference(
                                m_pDevice->PalDevice(deviceIdx),
                                pGpuMemory->groupShadowMemory.m_pPalMemory[deviceIdx],
                                memCreateFlags.readOnly);
                        }
                    }
                }
            }
        }
        VK_ASSERT(palMemOffset == palMemSize);

        if (palResult != Pal::Result::Success)
        {
            // Free the GPU memory object and system memory in case of failure
            pGpuMemory->groupMemory.Destroy(m_pDevice->VkInstance());
        }

        return PalToVkResult(palResult);
    }
    else
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
}

// =====================================================================================================================
// Free a base GPU memory object allocation that was created by the internal memory manager.
void InternalMemMgr::FreeBaseGpuMem(
    const InternalMemoryPool*        pGpuMemory)
{
    VK_ASSERT(pGpuMemory != nullptr);

    for (uint32_t deviceIdx = 0; deviceIdx < m_pDevice->NumPalDevices(); deviceIdx++)
    {
        // Remove memory object from the residency list
        if (pGpuMemory->groupMemory.m_pPalMemory[deviceIdx] != nullptr)
        {
            m_pDevice->RemoveMemReference(
                m_pDevice->PalDevice(deviceIdx),
                pGpuMemory->groupMemory.m_pPalMemory[deviceIdx]);
        }

        // Remove shadow group memory from the residency list
        if (pGpuMemory->groupShadowMemory.m_pPalMemory[deviceIdx] != nullptr)
        {
            m_pDevice->RemoveMemReference(
                m_pDevice->PalDevice(deviceIdx),
                pGpuMemory->groupShadowMemory.m_pPalMemory[deviceIdx]);
        }
    }

    // Free the GPU memory object and system memory used by the object
    pGpuMemory->groupMemory.Destroy(m_pDevice->VkInstance());
    pGpuMemory->groupShadowMemory.Destroy(m_pDevice->VkInstance());
}

// =====================================================================================================================
Pal::IGpuMemory* DeviceGroupMemory::PalMemory(int32_t idx) const
{
    VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
    return m_pPalMemory[idx];
}

// =====================================================================================================================
void* DeviceGroupMemory::CpuAddr(int32_t idx) const
{
    VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
    return m_pPersistentCpuAddr[idx];
}

// =====================================================================================================================
void DeviceGroupMemory::Destroy(Instance* pInstance) const
{
    void* pSystemMem = m_pPalMemory[0];

    for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; deviceIdx++)
    {
        if (m_pPalMemory[deviceIdx] != nullptr)
        {
            m_pPalMemory[deviceIdx]->Destroy();
        }
    }

    pInstance->FreeMem(pSystemMem);
}

// =====================================================================================================================
Pal::Result DeviceGroupMemory::Map()
{
    Pal::Result result = Pal::Result::ErrorNotMappable;

    for (uint32_t deviceIdx = 0; (deviceIdx < MaxPalDevices); deviceIdx++)
    {
        if (m_pPalMemory[deviceIdx] != nullptr)
        {
            if (m_pPalMemory[deviceIdx]->Map(&m_pPersistentCpuAddr[deviceIdx]) == Pal::Result::Success)
            {
                result = Pal::Result::Success;
            }
        }
    }
    return result;
}

// =====================================================================================================================
Pal::Result DeviceGroupMemory::Unmap() const
{
    Pal::Result result = Pal::Result::Success;

    for (uint32_t deviceIdx = 0;
        (deviceIdx < MaxPalDevices) && (result == Pal::Result::Success);
        deviceIdx++)
    {
        if ((m_pPalMemory[deviceIdx] != nullptr) &&
            (m_pPersistentCpuAddr[deviceIdx] != nullptr))
        {
            result = m_pPalMemory[deviceIdx]->Unmap();
        }
    }
    return result;
}

// =====================================================================================================================
void DeviceGroupMemory::GetVirtualAddress(Pal::gpusize* pGpuVA, Pal::gpusize memOffset)
{
    for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; deviceIdx++)
    {
        Pal::IGpuMemory* pPalMemory = PalMemory(deviceIdx);
        if (pPalMemory != nullptr)
        {
            pGpuVA[deviceIdx] = pPalMemory->Desc().gpuVirtAddr + memOffset;
        }
    }
}

// =====================================================================================================================
// Maps an internal memory sub-allocation
Pal::Result InternalMemory::Map(
    uint32_t idx,
    void**   pCpuAddr)
{
    Pal::Result result = Pal::Result::Success;

    if (m_memoryPool.groupMemory.CpuAddr(idx) != nullptr)
    {
        *pCpuAddr = m_memoryPool.groupMemory.CpuAddr(idx);
    }
    else
    {
        result = PalMemory(idx)->Map(pCpuAddr);
    }

    if (result == Pal::Result::Success)
    {
        *pCpuAddr = Util::VoidPtrInc(*pCpuAddr, static_cast<size_t>(m_offset));
    }

    return result;
}

// =====================================================================================================================
// Maps an internal memory sub-allocation
Pal::Result InternalMemory::ShadowMap(
    uint32_t idx,
    void**   ppCpuAddr)
{
    Pal::Result result = Pal::Result::Success;

    if (m_memoryPool.groupShadowMemory.CpuAddr(idx) != nullptr)
    {
        *ppCpuAddr = m_memoryPool.groupShadowMemory.CpuAddr(idx);
    }
    else
    {
        Pal::IGpuMemory*  pPalMemory = m_memoryPool.groupShadowMemory.PalMemory(idx);

        if (pPalMemory != nullptr)
        {
            pPalMemory->Map(ppCpuAddr);
        }
    }

    if ((result == Pal::Result::Success) && ((*ppCpuAddr) != nullptr))
    {
        *ppCpuAddr = Util::VoidPtrInc(*ppCpuAddr, static_cast<size_t>(m_offset));
    }

    return result;
}

// =====================================================================================================================
// Unmaps an internal memory sub-allocation
Pal::Result InternalMemory::Unmap(uint32_t idx)
{
    if (m_memoryPool.groupMemory.CpuAddr(idx) == nullptr)
    {
        return PalMemory(idx)->Unmap();
    }
    else
    {
        return Pal::Result::Success;
    }
}

} // namespace vk
