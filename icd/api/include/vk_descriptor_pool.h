/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  vk_descriptor_pool.h
 * @brief Functionality related to Vulkan descriptor pool objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_POOL_H__
#define __VK_DESCRIPTOR_POOL_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_alloccb.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/vk_memory.h"
#include "include/internal_mem_mgr.h"

namespace vk
{

class DescriptorSetLayout;
class DescriptorSet;
class DescriptorPool;

// =====================================================================================================================
// This class manages GPU memory for descriptor sets.  It is owned by DescriptorPool.
class DescriptorGpuMemHeap
{
public:
    DescriptorGpuMemHeap();

    VkResult Init(
        Device*                          pDevice,
        VkDescriptorPoolCreateFlags      poolUsage,
        uint32_t                         maxSets,
        const uint32_t                   count,
        const VkDescriptorPoolSize*      pTypeCount);

    void Destroy(Device* pDevice);

    bool AllocSetGpuMem(
        const DescriptorSetLayout*  pLayout,
        Pal::gpusize*               pSetGpuMemOffset,
        void**                      pSetAllocHandle);

    void GetGpuMemRequirements(
        Pal::GpuMemoryRequirements* pGpuMemReqs);

    VkResult BindMemory(
        InternalMemory*         pInternalMem);

    void FreeSetGpuMem(
        void*        pSetAllocHandle);

    void Reset();

    VK_INLINE void* CpuAddr(uint32_t deviceIdx) const
        { return m_pCpuAddr[deviceIdx]; }

    void* GetDescriptorSetMappedAddress(
        uint32_t     deviceIdx,
        void*        pSetAllocHandle,
        Pal::gpusize setGpuOffset);

protected:
    struct DynamicAllocBlock
    {
        DynamicAllocBlock*    pPrevFree;                // Address of the previous free block (null for non-free blocks)
        DynamicAllocBlock*    pNextFree;                // Address of next free block (null for non-free blocks)
        DynamicAllocBlock*    pPrev;                    // Address of previous block
        DynamicAllocBlock*    pNext;                    // Address of next block
        Pal::gpusize          gpuMemOffsetRangeStart;   // Start of GPU address range of this block
        Pal::gpusize          gpuMemOffsetRangeEnd;     // End of GPU address range of this block
    };

    bool IsDynamicAllocBlockFree(const DynamicAllocBlock* pBlock) const
    {
        // pPrevFree is never null for free blocks as they are chained after a list header so this is how we determine
        // whether a block is free. Also, we consider null as a non-free block for simplicity.
        return (pBlock != nullptr) && (pBlock->pPrevFree != nullptr);
    }

    uint32_t DynamicAllocBlockIndex(const DynamicAllocBlock* pBlock) const
    {
        // Calculate the index of the block within the block storage using pointer arithmetics.
        return static_cast<uint32_t>(Util::VoidPtrDiff(pBlock, m_pDynamicAllocBlocks) / sizeof(DynamicAllocBlock));
    }

#if DEBUG
    void SanityCheckDynamicAllocBlockList();
#endif

    VkDescriptorPoolCreateFlags     m_usage;                  // Pool usage

    Pal::gpusize              m_oneShotAllocForward;    // Start of free memory for one-shot allocs (allocated forwards)

    DynamicAllocBlock         m_dynamicAllocBlockFreeListHeader;    // Header for the list of free blocks
    DynamicAllocBlock*        m_pDynamicAllocBlocks;                // Storage of block structures
    uint32_t                  m_dynamicAllocBlockCount;             // Number of block structures
    uint32_t*                 m_pDynamicAllocBlockIndexStack;       // Stack of indices of available block structures
    uint32_t                  m_dynamicAllocBlockIndexStackCount;   // Number of available block structures

    InternalMemory            m_internalMem;
    Pal::gpusize              m_gpuMemSize;                         // Required GPU memory size
    uint32_t                  m_gpuMemAddrAlignment;                // Required GPU memory address alignment of descriptor sets

    uint32_t                  m_numPalDevices;                      // Number of Pal devices handed by this Heap class
    Pal::gpusize              m_gpuMemOffsetRangeStart;             // Start of bound GPU address range
    Pal::gpusize              m_gpuMemOffsetRangeEnd;               // End of bound GPU address range
    void*                     m_pCpuAddr[MaxPalDevices];            // The mapped Cpu addresses
};

// =====================================================================================================================
// This class manages CPU state memory for VkDescriptorSet instances.  It is owned by DescriptorPool.
class DescriptorSetHeap
{
public:
    DescriptorSetHeap();

    VkResult Init(
        Device*                         pDevice,
        DescriptorPool*                 pPool,
        VkDescriptorPoolCreateFlags     poolUsage,
        uint32_t                        maxSets);

    void Destroy(Device* pDevice);

    bool AllocSetState(VkDescriptorSet* pSet);

    void FreeSetState(VkDescriptorSet set);

    void Reset();

private:
    uint32_t             m_nextFreeHandle;
    uint32_t             m_maxSets;
    VkDescriptorSet*     m_pHandles;

    uint32_t*            m_pFreeIndexStack;
    uint32_t             m_freeIndexStackCount;

    void*                m_pSetMemory;
};

// =====================================================================================================================
// API implementation of Vulkan descriptor pools (VkDescriptorPool).  These pools manage GPU memory and driver state
// memory for instances of VkDescriptorSet objects.
class DescriptorPool : public NonDispatchable<VkDescriptorPool, DescriptorPool>
{
public:
    static VkResult Create(
        Device*                                pDevice,
        VkDescriptorPoolCreateFlags            poolUsage,
        uint32_t                               maxSets,
        const VkDescriptorPoolCreateInfo*      pCreateInfo,
        const VkAllocationCallbacks*           pAllocator,
        VkDescriptorPool*                      pDescriptorPool);

    VkResult Destroy(
        Device*                                pDevice,
        const VkAllocationCallbacks*           pAllocator);

    VkResult Reset();

    VkResult AllocDescriptorSets(
        uint32_t                        count,
        const VkDescriptorSetLayout*    pSetLayouts,
        VkDescriptorSet*                pDescriptorSets);

    VkResult FreeDescriptorSets(
        uint32_t                         count,
        const VkDescriptorSet*           pDescriptorSets);

    void* GetDescriptorSetMappedAddress(
        uint32_t       deviceIdx,
        Pal::gpusize   gpuMemOffset,
        DescriptorSet* pSet);

    Device* VkDevice() const { return m_pDevice; }

private:
    VkResult Init(
        Device*                               pDevice,
        VkDescriptorPoolCreateFlags           poolUsage,
        uint32_t                              maxSets,
        const VkDescriptorPoolCreateInfo*     pCreateInfo);

    DescriptorPool(Device* pDevice);

    Device*              m_pDevice;         // Device pointer
    DescriptorSetHeap    m_setHeap;         // Allocates driver state instances of descriptor sets
    DescriptorGpuMemHeap m_gpuMemHeap;      // Allocates GPU memory for descriptor sets
    InternalMemory       m_internalMem;     // Internal GPU memory allocation for the descriptor pool
    Pal::gpusize         m_gpuAddressCached[MaxPalDevices];
    uint32_t*            m_pCpuAddressCached[MaxPalDevices];
};

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets);

VKAPI_ATTR VkResult VKAPI_CALL vkResetDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorPoolResetFlags                  flags);

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice                                    device,
    const VkDescriptorSetAllocateInfo*          pAllocateInfo,
    VkDescriptorSet*                            pDescriptorSets);

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    const VkAllocationCallbacks*                pAllocator);
}

} // namespace vk

#endif /* __VK_DESCRIPTOR_POOL_H__ */
