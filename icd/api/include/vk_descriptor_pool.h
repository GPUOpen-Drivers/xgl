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
#include "include/vk_descriptor_set.h"

namespace vk
{

class DescriptorSetLayout;
class DescriptorPool;

// =====================================================================================================================
// This class manages GPU memory for descriptor sets.  It is owned by DescriptorPool.
class DescriptorGpuMemHeap
{
public:
    DescriptorGpuMemHeap();

    VkResult Init(
        Device*                                     pDevice,
        const VkDescriptorPoolCreateInfo*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator);

    void Destroy(
        Device* pDevice,
        const VkAllocationCallbacks* pAllocator);

    bool AllocSetGpuMem(
        const DescriptorSetLayout*  pLayout,
        uint32_t                    variableDescriptorCounts,
        Pal::gpusize*               pSetGpuMemOffset,
        void**                      pSetAllocHandle);

    void GetGpuMemRequirements(
        Pal::GpuMemoryRequirements* pGpuMemReqs);

    VkResult BindMemory(
        InternalMemory*         pInternalMem);

    VkResult SetupCPUOnlyMemory(
        void*        pCpuMem);

    void FreeSetGpuMem(
        void*        pSetAllocHandle);

    void Reset();

    void* CpuAddr(uint32_t deviceIdx) const
        { return m_pCpuAddr[deviceIdx]; }

    void* CpuShadowAddr(uint32_t deviceIdx) const
        { return m_pCpuShadowAddr[deviceIdx]; }

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

    InternalMemory*           m_pInternalMem;
    Pal::gpusize              m_gpuMemSize;                         // Required GPU memory size
    uint32_t                  m_gpuMemAddrAlignment;                // Required GPU memory address alignment of descriptor sets

    uint32_t                  m_numPalDevices;                      // Number of Pal devices handed by this Heap class
    Pal::gpusize              m_gpuMemOffsetRangeStart;             // Start of bound GPU address range
    Pal::gpusize              m_gpuMemOffsetRangeEnd;               // End of bound GPU address range

    void*                     m_pCpuAddr[MaxPalDevices];            // The mapped Cpu addresses
    void*                     m_pCpuShadowAddr[MaxPalDevices];      // The mapped Shadow Cpu addresses

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorGpuMemHeap);
};

// =====================================================================================================================
// This class manages CPU state memory for VkDescriptorSet instances.  It is owned by DescriptorPool.
class DescriptorSetHeap
{
public:
    DescriptorSetHeap();

    template <uint32_t numPalDevices>
    VkResult Init(
        Device*                           pDevice,
        const VkAllocationCallbacks*      pAllocator,
        const VkDescriptorPoolCreateInfo* pCreateInfo);

    void Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    template <uint32_t numPalDevices>
    bool AllocSetState(VkDescriptorSet* pSet);

    template <uint32_t numPalDevices>
    void FreeSetState(VkDescriptorSet set);

    template <uint32_t numPalDevices>
    void Reset();

    size_t GetPrivateDataSize() const
    {
        return m_privateDataSize;
    }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorSetHeap);

    template <uint32_t numPalDevices>
    VkDescriptorSet DescriptorSetHandleFromIndex(uint32_t idx) const;

    uint32_t             m_nextFreeHandle;
    uint32_t             m_maxSets;

    uint32_t*            m_pFreeIndexStack;
    uint32_t             m_freeIndexStackCount;

    size_t               m_privateDataSize;
    size_t               m_setSize;

    void*                m_pSetMemory;
};

// =====================================================================================================================
// API implementation of Vulkan descriptor pools (VkDescriptorPool).  These pools manage GPU memory and driver state
// memory for instances of VkDescriptorSet objects.
class DescriptorPool final : public NonDispatchable<VkDescriptorPool, DescriptorPool>
{
public:
    template <uint32_t numPalDevices>
    static VkResult Create(
        Device*                                pDevice,
        const VkDescriptorPoolCreateInfo*      pCreateInfo,
        const VkAllocationCallbacks*           pAllocator,
        VkDescriptorPool*                      pDescriptorPool);

    VkResult Destroy(
        Device*                                pDevice,
        const VkAllocationCallbacks*           pAllocator);

    template <uint32_t numPalDevices>
    VkResult Reset();

    template <uint32_t numPalDevices>
    VkResult AllocDescriptorSets(
        const VkDescriptorSetAllocateInfo* pAllocateInfo,
        VkDescriptorSet*                   pDescriptorSets);

    template <uint32_t numPalDevices>
    VkResult FreeDescriptorSets(
        uint32_t                         count,
        const VkDescriptorSet*           pDescriptorSets);

    static PFN_vkCreateDescriptorPool GetCreateDescriptorPoolFunc(Device* pDevice);

    static PFN_vkFreeDescriptorSets GetFreeDescriptorSetsFunc(Device* pDevice);

    static PFN_vkResetDescriptorPool GetResetDescriptorPoolFunc(Device* pDevice);

    static PFN_vkAllocateDescriptorSets GetAllocateDescriptorSetsFunc(Device* pDevice);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorPool);

    template <uint32_t numPalDevices>
    VkResult Init(
        Device*                               pDevice,
        const VkDescriptorPoolCreateInfo*     pCreateInfo,
        const VkAllocationCallbacks*          pAllocator);

    DescriptorPool(Device* pDevice);

    template <uint32_t numPalDevices>
    static VKAPI_ATTR VkResult VKAPI_CALL CreateDescriptorPool(
        VkDevice                                    device,
        const VkDescriptorPoolCreateInfo*           pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDescriptorPool*                           pDescriptorPool);

    template <uint32_t numPalDevices>
    static VKAPI_ATTR VkResult VKAPI_CALL FreeDescriptorSets(
        VkDevice                                    device,
        VkDescriptorPool                            descriptorPool,
        uint32_t                                    descriptorSetCount,
        const VkDescriptorSet*                      pDescriptorSets);

    template <uint32_t numPalDevices>
    static VKAPI_ATTR VkResult VKAPI_CALL ResetDescriptorPool(
        VkDevice                                    device,
        VkDescriptorPool                            descriptorPool,
        VkDescriptorPoolResetFlags                  flags);

    template <uint32_t numPalDevices>
    static VKAPI_ATTR VkResult VKAPI_CALL AllocateDescriptorSets(
        VkDevice                                    device,
        const VkDescriptorSetAllocateInfo*          pAllocateInfo,
        VkDescriptorSet*                            pDescriptorSets);

    Device*              m_pDevice;           // Device pointer
    DescriptorSetHeap    m_setHeap;           // Allocates driver state instances of descriptor sets
    DescriptorGpuMemHeap m_gpuMemHeap;        // Allocates GPU memory for descriptor sets

    InternalMemory       m_staticInternalMem; // Static Internal GPU memory

    bool                 m_DynamicDataSupport; // Pool supports dynamic data
    bool                 m_hostOnly;          // VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT flag set
    void*                m_pHostOnlyMemory;   // Memory allocated for host only descriptor pools

    DescriptorAddr       m_addresses[MaxPalDevices];

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
