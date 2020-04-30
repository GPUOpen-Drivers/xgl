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
 ***********************************************************************************************************************
 * @file  vk_descriptor.cpp
 * @brief Contains implementation of Vulkan descriptor pool objects.
 ***********************************************************************************************************************
 */

#include "include/vk_descriptor_pool.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_dispatch.h"
#include "include/vk_object.h"
#include "include/vk_instance.h"
#include "include/vk_utils.h"
#include "include/vk_queue.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_descriptor_set.h"

#include "palInlineFuncs.h"
#include "palDevice.h"
#include "palEventDefs.h"
#include "palGpuMemory.h"

namespace vk
{

using namespace Pal;

// =====================================================================================================================
// Creates a descriptor region
template <uint32_t numPalDevices>
VkResult DescriptorPool::Create(
    Device*                                  pDevice,
    const VkDescriptorPoolCreateInfo*        pCreateInfo,
    const VkAllocationCallbacks*             pAllocator,
    VkDescriptorPool*                        pDescriptorPool)
{
    const size_t apiSize = sizeof(DescriptorPool);
    const size_t objSize = apiSize;

    void* pSysMem = pAllocator->pfnAllocation(
        pAllocator->pUserData,
        objSize,
        VK_DEFAULT_MEM_ALIGN,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (pSysMem == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VK_PLACEMENT_NEW (pSysMem) DescriptorPool(pDevice);

    VkDescriptorPool handle = DescriptorPool::HandleFromVoidPointer(pSysMem);

    VkResult result = DescriptorPool::ObjectFromHandle(handle)->Init<numPalDevices>(pDevice, pCreateInfo);

    if (result == VK_SUCCESS)
    {
        *pDescriptorPool = handle;
    }
    else
    {
        DescriptorPool::ObjectFromHandle(handle)->Destroy(pDevice, pAllocator);
    }

    return result;
}

// =====================================================================================================================
DescriptorPool::DescriptorPool(
    Device* pDevice)
    :
    m_pDevice(pDevice)
{
    memset(m_addresses, 0, sizeof(m_addresses));
}

// =====================================================================================================================
// Initializes a DescriptorPool.
template <uint32_t numPalDevices>
VkResult DescriptorPool::Init(
    Device*                                pDevice,
    const VkDescriptorPoolCreateInfo*      pCreateInfo)
{
    VkDescriptorPoolCreateFlags poolUsage   = pCreateInfo->flags;
    uint32_t                    maxSets     = pCreateInfo->maxSets;

    VkResult result = VK_SUCCESS;

    result = m_setHeap.Init<numPalDevices>(pDevice, poolUsage, maxSets);

    if (result == VK_SUCCESS)
    {
        result = m_gpuMemHeap.Init(pDevice, poolUsage, maxSets, pCreateInfo->poolSizeCount, pCreateInfo->pPoolSizes);

        if (result != VK_SUCCESS)
        {
            return result;
        }

        // Get memory requirements
        Pal::GpuMemoryRequirements memReqs = {};

        m_gpuMemHeap.GetGpuMemRequirements(&memReqs);

        if (memReqs.size > 0)
        {
            InternalMemCreateInfo allocInfo = {};

            allocInfo.pal.size      = memReqs.size;
            allocInfo.pal.alignment = memReqs.alignment;
            allocInfo.pal.priority  = m_pDevice->GetRuntimeSettings().enableHighPriorityDescriptorMemory ?
                                        Pal::GpuMemPriority::High :
                                        Pal::GpuMemPriority::Normal;

            pDevice->MemMgr()->GetCommonPool(InternalPoolDescriptorTable, &allocInfo);

            allocInfo.flags.needShadow = m_pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead;

            result = pDevice->MemMgr()->AllocGpuMem(allocInfo, &m_staticInternalMem, pDevice->GetPalDeviceMask());

            if (result != VK_SUCCESS)
            {
                return result;
            }

            m_gpuMemHeap.BindMemory(&m_staticInternalMem);

            for (uint32_t deviceIdx = 0; deviceIdx < MaxPalDevices; deviceIdx++)
            {
                m_addresses[deviceIdx].staticGpuAddr = m_staticInternalMem.GpuVirtAddr(deviceIdx);
                m_addresses[deviceIdx].staticCpuAddr = static_cast<uint32_t*>(m_gpuMemHeap.CpuAddr(deviceIdx));

                if (m_pDevice->GetRuntimeSettings().enableFmaskBasedMsaaRead)
                {
                    m_addresses[deviceIdx].fmaskGpuAddr = m_staticInternalMem.GpuShadowVirtAddr(deviceIdx);
                    m_addresses[deviceIdx].fmaskCpuAddr = static_cast<uint32_t*>(m_gpuMemHeap.CpuShadowAddr(deviceIdx));
                }
            }
        }
    }

    if (result == VK_SUCCESS)
    {
        uint32 memRequired = sizeof(Pal::ResourceDescriptionDescriptorPool) +
                             (sizeof(Pal::ResourceDescriptionPoolSize) * pCreateInfo->poolSizeCount);
        void* pMem =
            m_pDevice->VkInstance()->AllocMem(memRequired, VkSystemAllocationScope::VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pMem != nullptr)
        {
            // Log the creation of the descriptor pool and the binding of GPU memory to it
            Pal::ResourceDescriptionDescriptorPool* pDesc = static_cast<Pal::ResourceDescriptionDescriptorPool*>(pMem);

            Pal::ResourceDescriptionPoolSize* pPoolSizes = static_cast<Pal::ResourceDescriptionPoolSize*>(
                Util::VoidPtrInc(pMem, sizeof(Pal::ResourceDescriptionDescriptorPool)));

            pDesc->maxSets     = pCreateInfo->maxSets;
            pDesc->numPoolSize = pCreateInfo->poolSizeCount;
            pDesc->pPoolSizes  = pPoolSizes;

            for (uint32 i = 0; i < pCreateInfo->poolSizeCount; i++)
            {
                pPoolSizes[i].type = VkDescriptorTypeToPalDescriptorType(pCreateInfo->pPoolSizes[i].type);
                pPoolSizes[i].numDescriptors = pCreateInfo->pPoolSizes[i].descriptorCount;
            }

            Pal::ResourceCreateEventData data = {};
            data.type              = Pal::ResourceType::DescriptorPool;
            data.pResourceDescData = pDesc;
            data.resourceDescSize  = sizeof(Pal::ResourceDescriptionDescriptorPool);
            data.pObj              = this;

            pDevice->VkInstance()->PalPlatform()->LogEvent(
                Pal::PalEvent::GpuMemoryResourceCreate,
                &data,
                sizeof(Pal::ResourceCreateEventData));

            m_pDevice->VkInstance()->FreeMem(pMem);

            Pal::GpuMemoryResourceBindEventData bindData = {};
            bindData.pObj               = this;
            bindData.pGpuMemory         = m_staticInternalMem.PalMemory(DefaultDeviceIndex);
            bindData.requiredGpuMemSize = m_staticInternalMem.Size();
            bindData.offset             = m_staticInternalMem.Offset();

            pDevice->VkInstance()->PalPlatform()->LogEvent(
                Pal::PalEvent::GpuMemoryResourceBind,
                &bindData,
                sizeof(Pal::GpuMemoryResourceBindEventData));
        }
    }

    return result;
}

// =====================================================================================================================
// Resets the entire descriptor pool.  All storage becomes free for allocation and all previously allocated descriptor
// sets become invalid.
template <uint32_t numPalDevices>
VkResult DescriptorPool::Reset()
{
    m_setHeap.Reset<numPalDevices>();
    m_gpuMemHeap.Reset();

    return VK_SUCCESS;
}

// =====================================================================================================================
// Destroys a descriptor pool
VkResult DescriptorPool::Destroy(
    Device*                         pDevice,
    const VkAllocationCallbacks*    pAllocator)
{
    Pal::ResourceDestroyEventData data = {};
    data.pObj = m_staticInternalMem.PalMemory(DefaultDeviceIndex);

    pDevice->VkInstance()->PalPlatform()->LogEvent(
        Pal::PalEvent::GpuMemoryResourceDestroy,
        &data,
        sizeof(Pal::ResourceDestroyEventData));

    // Destroy children heaps
    m_setHeap.Destroy(pDevice);
    m_gpuMemHeap.Destroy(pDevice);

    // Free internal GPU memory allocation used by the object
    if (m_staticInternalMem.PalMemory(DefaultDeviceIndex) != nullptr)
    {
        pDevice->MemMgr()->FreeGpuMem(&m_staticInternalMem);
    }

    // Call destructor
    this->~DescriptorPool();

    // Free memory
    pAllocator->pfnFree(pAllocator->pUserData, this);

    // Cannot fail
    return VK_SUCCESS;
}

// =====================================================================================================================
// Allocate descriptor sets from a descriptor set region.
template <uint32_t numPalDevices>
VkResult DescriptorPool::AllocDescriptorSets(
    const VkDescriptorSetAllocateInfo* pAllocateInfo,
    VkDescriptorSet*                   pDescriptorSets)
{
    VkResult                     result                          = VK_SUCCESS;
    uint32_t                     allocCount                      = 0;
    uint32_t                     count                           = pAllocateInfo->descriptorSetCount;
    const VkDescriptorSetLayout* pSetLayouts                     = pAllocateInfo->pSetLayouts;

    const VkDescriptorSetVariableDescriptorCountAllocateInfo* pVariableDescriptorCount =
        reinterpret_cast<const VkDescriptorSetVariableDescriptorCountAllocateInfo*>(pAllocateInfo->pNext);

    while ((result == VK_SUCCESS) && (allocCount < count))
    {
        if (m_setHeap.AllocSetState<numPalDevices>(&pDescriptorSets[allocCount]))
        {
            // Try to allocate GPU memory for the descriptor set
            DescriptorSetLayout* pLayout = DescriptorSetLayout::ObjectFromHandle(pSetLayouts[allocCount]);

            uint32_t variableDescriptorCounts = 0;

            // Get variable descriptor counts for the last layout binding
            if (pVariableDescriptorCount != nullptr)
            {
                VK_ASSERT(pVariableDescriptorCount->sType ==
                    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO);

                VK_ASSERT(pVariableDescriptorCount->descriptorSetCount == pAllocateInfo->descriptorSetCount);

                uint32_t lastBindingIdx = pLayout->Info().count - 1;

                if (pLayout->Binding(lastBindingIdx).bindingFlags &
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
                {
                    variableDescriptorCounts = pVariableDescriptorCount->pDescriptorCounts[allocCount];
                    VK_ASSERT(variableDescriptorCounts <= pLayout->Binding(lastBindingIdx).info.descriptorCount);
                }
            }

            Pal::gpusize setGpuMemOffset;
            void* pSetAllocHandle;

            if (m_gpuMemHeap.AllocSetGpuMem(pLayout, variableDescriptorCounts, &setGpuMemOffset, &pSetAllocHandle))
            {
                // Allocation succeeded: Mark this
                // Reallocate this descriptor set to use the allocated GPU range and layout
                DescriptorSet<numPalDevices>* pSet = DescriptorSet<numPalDevices>::StateFromHandle(pDescriptorSets[allocCount]);

                pSet->Reassign(pLayout,
                               setGpuMemOffset,
                               m_addresses,
                               pSetAllocHandle);
            }
            else
            {
                // State set will be released in error case handling below, since non-null handle is present
                result = VK_ERROR_OUT_OF_POOL_MEMORY;
            }

            allocCount++;
        }
        else
        {
            result = VK_ERROR_OUT_OF_POOL_MEMORY;
        }
    }

    if (result != VK_SUCCESS)
    {
        for (uint32_t setIdx = 0; setIdx < count; ++setIdx)
        {
            // For any descriptor set that we have allocated, release its state set and any associated GPU memory
            if (setIdx < allocCount)
            {
                DescriptorSet<numPalDevices>* pSet =
                    DescriptorSet<numPalDevices>::StateFromHandle(pDescriptorSets[setIdx]);

                m_gpuMemHeap.FreeSetGpuMem(pSet->AllocHandle());

                m_setHeap.FreeSetState<numPalDevices>(pDescriptorSets[setIdx]);
            }

            // No partial failures allowed for creating multiple descriptor sets. Update all to VK_NULL_HANDLE.
            pDescriptorSets[setIdx] = VK_NULL_HANDLE;
        }
    }

    return result;
}

// =====================================================================================================================
// Frees an individual descriptor set after it has been destroyed.
template <uint32_t numPalDevices>
VkResult DescriptorPool::FreeDescriptorSets(
    uint32_t                         count,
    const VkDescriptorSet*           pDescriptorSets)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        if (pDescriptorSets[i] == VK_NULL_HANDLE)
        {
            continue;
        }

        // Free this set's GPU memory
        DescriptorSet<numPalDevices>* pSet  = DescriptorSet<numPalDevices>::StateFromHandle(pDescriptorSets[i]);
        m_gpuMemHeap.FreeSetGpuMem(pSet->AllocHandle());

        // Free this set's state
        m_setHeap.FreeSetState<numPalDevices>(pDescriptorSets[i]);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
DescriptorGpuMemHeap::DescriptorGpuMemHeap() :
m_usage(0),
m_oneShotAllocForward(0),
m_pDynamicAllocBlocks(nullptr),
m_dynamicAllocBlockCount(0),
m_pDynamicAllocBlockIndexStack(nullptr),
m_dynamicAllocBlockIndexStackCount(0),
m_gpuMemSize(0),
m_gpuMemAddrAlignment(0),
m_numPalDevices(0)
{
    m_gpuMemOffsetRangeStart = 0;
    m_gpuMemOffsetRangeEnd   = 0;

    memset(m_pCpuAddr, 0, sizeof(m_pCpuAddr));
    memset(m_pCpuShadowAddr, 0, sizeof(m_pCpuShadowAddr));
}

// =====================================================================================================================
// Initializes a DescriptorGpuMemHeap.  Allocates any internal GPU memory for it if needed.
VkResult DescriptorGpuMemHeap::Init(
    Device*                      pDevice,
    VkDescriptorPoolCreateFlags  poolUsage,
    uint32_t                     maxSets,
    const uint32_t               count,
    const VkDescriptorPoolSize*  pTypeCount)
{
    m_numPalDevices = pDevice->NumPalDevices();
    m_usage      = poolUsage;
    m_gpuMemSize = 0;

    bool oneShot = (m_usage & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == 0;

    for (uint32_t i = 0; i < count; ++i)
    {
        m_gpuMemSize += DescriptorSetLayout::GetSingleDescStaticSize(pDevice, pTypeCount[i].type) *
            pTypeCount[i].descriptorCount;
    }

    m_gpuMemAddrAlignment = pDevice->GetProperties().descriptorSizes.alignment;

    if (oneShot == false) //DYNAMIC USAGE
    {
        // In case of dynamic descriptor pools we have to prepare our management structures.
        // There can be at most maxSets * 2 + 1 blocks in a pool.
        m_dynamicAllocBlockCount    = (maxSets * 2 + 1);
        size_t blockStorageSize     = m_dynamicAllocBlockCount * sizeof(DynamicAllocBlock);
        size_t blockIndexStackSize  = m_dynamicAllocBlockCount * sizeof(uint32_t);

        // Allocate system memory for the management structures
        void* pMemory = pDevice->VkInstance()->AllocMem(
            blockStorageSize + blockIndexStackSize,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_DEVICE);

        if (pMemory == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        // Initialize the management structures
        m_dynamicAllocBlockFreeListHeader.pPrevFree = nullptr;
        m_dynamicAllocBlockFreeListHeader.pNextFree = nullptr;
        m_dynamicAllocBlockFreeListHeader.pPrev     = nullptr;
        m_dynamicAllocBlockFreeListHeader.pNext     = nullptr;

        m_pDynamicAllocBlocks               = reinterpret_cast<DynamicAllocBlock*>(pMemory);
        m_pDynamicAllocBlockIndexStack      = reinterpret_cast<uint32_t*>(Util::VoidPtrInc(pMemory, blockStorageSize));
        m_dynamicAllocBlockIndexStackCount  = m_dynamicAllocBlockCount;

        for (uint32_t i = 0; i < m_dynamicAllocBlockIndexStackCount; ++i)
        {
            m_pDynamicAllocBlockIndexStack[i] = i;
        }
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Destroys a DescriptorGpuMemHeap
void DescriptorGpuMemHeap::Destroy(
    Device* pDevice)
{
    for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        if (m_pCpuAddr[deviceIdx] != nullptr)
        {
            m_internalMem.Unmap(deviceIdx);
        }
    }

    if (m_pDynamicAllocBlocks != nullptr)
    {
        pDevice->VkInstance()->FreeMem(m_pDynamicAllocBlocks);
    }
}

#if DEBUG
// =====================================================================================================================
// Sanity checks the block lists in a debug driver.
void DescriptorGpuMemHeap::SanityCheckDynamicAllocBlockList()
{
    uint32_t            blockCount  = 0;
    DynamicAllocBlock*  pBlock      = nullptr;
    DynamicAllocBlock*  pPrevBlock  = nullptr;

    // Sanity check the free block list.
    pPrevBlock = &m_dynamicAllocBlockFreeListHeader;
    pBlock = m_dynamicAllocBlockFreeListHeader.pNextFree;
    blockCount = 0;
    while (pBlock != nullptr)
    {
        blockCount++;

        // The number of blocks in the free list should not exceed half of the blocks, otherwise that's an indication
        // of a loop in the list of free blocks.
        VK_ASSERT(blockCount <= (m_dynamicAllocBlockCount / 2 + 1));

        // The pPrevFree field should point to the previous block in the free list.
        VK_ASSERT(pBlock->pPrevFree == pPrevBlock);

        pPrevBlock = pBlock;
        pBlock = pBlock->pNextFree;
    }

    // Find the first node in the complete block list.
    pBlock = nullptr;
    for (uint32_t i = 0; i < m_dynamicAllocBlockCount; ++i)
    {
        if (m_pDynamicAllocBlocks[i].pPrev == nullptr)
        {
            // Make sure that this is not one of the unused blocks
            bool unused = false;
            for (uint32_t j = 0; j < m_dynamicAllocBlockIndexStackCount; ++j)
            {
                if (m_pDynamicAllocBlockIndexStack[j] == i)
                {
                    unused = true;
                    break;
                }
            }

            if (!unused)
            {
                // We've found the first item, remember it.
                pBlock = &m_pDynamicAllocBlocks[i];
                break;
            }
        }
    }

    // If we didn't find the first node of the list then something went wrong.
    VK_ASSERT(pBlock != nullptr);

    // The first block's start offset should match the pool's start offset
    VK_ASSERT(pBlock->gpuMemOffsetRangeStart == m_gpuMemOffsetRangeStart);

    // Sanity check the complete block list.
    pPrevBlock = pBlock;
    pBlock = pBlock->pNext;
    blockCount = 1;
    while (pBlock != nullptr)
    {
        blockCount++;

        // The number of blocks in the list should not exceed the total number of the blocks, otherwise that's an
        // indication of a loop in the list of free blocks.
        VK_ASSERT(blockCount <= m_dynamicAllocBlockCount);

        // The pPrev field should point to the previous block in the list.
        VK_ASSERT(pBlock->pPrev == pPrevBlock);

        // The start of this block should match the end of the previous block in the list.
        VK_ASSERT(pBlock->gpuMemOffsetRangeStart == pPrevBlock->gpuMemOffsetRangeEnd);

        pPrevBlock = pBlock;
        pBlock = pBlock->pNext;
    }

    // The last block's end offset should match the pool's end offset
    VK_ASSERT(pPrevBlock->gpuMemOffsetRangeEnd == m_gpuMemOffsetRangeEnd);
}
#endif

// =====================================================================================================================
// Allocates enough GPU memory to contain the given descriptor set layout.  Returns back a GPU VA offset and an opaque
// handle that can be used to free that memory for non-one-shot allocations.
bool DescriptorGpuMemHeap::AllocSetGpuMem(
    const DescriptorSetLayout*  pLayout,
    uint32_t                    variableDescriptorCounts,
    Pal::gpusize*               pSetGpuMemOffset,
    void**                      pSetAllocHandle)
{
    // Figure out the byte size and alignment
    uint32_t byteSize = 0;
    if (variableDescriptorCounts > 0)
    {
        uint32_t lastBindingIdx      = pLayout->Info().count - 1;
        uint32_t varBindingStaDWSize = pLayout->Binding(lastBindingIdx).sta.dwSize;

        // Total size = STA section size - last binding STA size + last binding variable descriptor count size
        byteSize = (pLayout->Info().sta.dwSize - varBindingStaDWSize) * sizeof(uint32_t) +
                   (pLayout->Info().varDescStride * variableDescriptorCounts);
    }
    else
    {
        byteSize = pLayout->Info().sta.dwSize * sizeof(uint32_t);
    }

    const uint32_t alignment = m_gpuMemAddrAlignment;

    if (byteSize == 0)
    {
        *pSetAllocHandle  = nullptr;
        *pSetGpuMemOffset = 0;

        return true;
    }
    bool oneShot = (m_usage & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == 0;

    // For one-shot allocations, allocate forwards from the one-shot range until you hit the dynamic range
    if (oneShot)
    {
        const Pal::gpusize gpuBaseOffset = Util::Pow2Align(m_oneShotAllocForward, alignment);

        if ((gpuBaseOffset + byteSize) <= m_gpuMemSize)
        {
            *pSetAllocHandle  = nullptr;
            *pSetGpuMemOffset = m_gpuMemOffsetRangeStart + gpuBaseOffset;

            m_oneShotAllocForward = gpuBaseOffset + byteSize;

            return true;
        }
    }
    // For dynamic allocations, do something more complicated
    else
    {
        DynamicAllocBlock* pBlock = m_dynamicAllocBlockFreeListHeader.pNextFree;

        while (pBlock != nullptr)
        {
            Pal::gpusize gpuBaseOffset  = Util::Pow2Align(pBlock->gpuMemOffsetRangeStart, alignment);
            Pal::gpusize newBlockStart  = gpuBaseOffset + byteSize;

            if (newBlockStart <= pBlock->gpuMemOffsetRangeEnd)
            {
                *pSetAllocHandle  = pBlock;
                *pSetGpuMemOffset = gpuBaseOffset;

                // If there's space left in this block then let's remember it.
                if (newBlockStart < pBlock->gpuMemOffsetRangeEnd)
                {
                    // If the next block is a free one then attach the remaining range to it.
                    if (IsDynamicAllocBlockFree(pBlock->pNext))
                    {
                        VK_ASSERT(pBlock->gpuMemOffsetRangeEnd == pBlock->pNext->gpuMemOffsetRangeStart);

                        pBlock->pNext->gpuMemOffsetRangeStart = newBlockStart;
                    }
                    else
                    // Otherwise create a new free block for the remaining range.
                    {
                        VK_ASSERT(m_dynamicAllocBlockIndexStackCount > 0);
                        uint32_t newBlockIndex = m_pDynamicAllocBlockIndexStack[--m_dynamicAllocBlockIndexStackCount];

                        DynamicAllocBlock* pNewBlock      = &m_pDynamicAllocBlocks[newBlockIndex];
                        pNewBlock->pPrevFree              = pBlock;
                        pNewBlock->pNextFree              = pBlock->pNextFree;
                        pNewBlock->pPrev                  = pBlock;
                        pNewBlock->pNext                  = pBlock->pNext;
                        pNewBlock->gpuMemOffsetRangeStart = newBlockStart;
                        pNewBlock->gpuMemOffsetRangeEnd   = pBlock->gpuMemOffsetRangeEnd;

                        if (pNewBlock->pNextFree != nullptr)
                        {
                            pNewBlock->pNextFree->pPrevFree = pNewBlock;
                        }

                        if (pNewBlock->pNext != nullptr)
                        {
                            pNewBlock->pNext->pPrev = pNewBlock;
                        }

                        pBlock->pNextFree               = pNewBlock;
                        pBlock->pNext                   = pNewBlock;
                    }

                    // Truncate the block to the allocated size.
                    pBlock->gpuMemOffsetRangeEnd = newBlockStart;
                }

                // Unlink this block from the list of free blocks.

                pBlock->pPrevFree->pNextFree = pBlock->pNextFree;
                if (pBlock->pNextFree != nullptr)
                {
                    pBlock->pNextFree->pPrevFree = pBlock->pPrevFree;
                }

                pBlock->pNextFree   = nullptr;
                pBlock->pPrevFree   = nullptr;

#if DEBUG
                // Sanity check the lists after a successful allocation.
                SanityCheckDynamicAllocBlockList();
#endif

                return true;
            }

            // Advance to the next free block.
            pBlock      = pBlock->pNextFree;
        }
    }

    return false;
}

// =====================================================================================================================
// Returns the GPU memory requirements of a DescriptorGpuMemHeap.
void DescriptorGpuMemHeap::GetGpuMemRequirements(
    Pal::GpuMemoryRequirements* pGpuMemReqs)
{
    VK_ASSERT(pGpuMemReqs != nullptr);

    pGpuMemReqs->size       = m_gpuMemSize;
    pGpuMemReqs->alignment  = m_gpuMemAddrAlignment;
    pGpuMemReqs->heapCount = 3;
    pGpuMemReqs->heaps[0]  = GpuHeapLocal;
    pGpuMemReqs->heaps[1]  = GpuHeapGartUswc;
    pGpuMemReqs->heaps[2]  = GpuHeapGartCacheable;
}

// =====================================================================================================================
// Binds backing GPU memory for this heap.
VkResult DescriptorGpuMemHeap::BindMemory(
    InternalMemory*         pInternalMem)
{
    VkResult result = VK_SUCCESS;

    for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        if (m_pCpuAddr[deviceIdx] != nullptr)
        {
            m_internalMem.Unmap(deviceIdx);

            m_pCpuAddr[deviceIdx]       = nullptr;
            m_pCpuShadowAddr[deviceIdx] = nullptr;
        }
    }

    m_internalMem       = *pInternalMem;

    m_gpuMemOffsetRangeStart = 0;
    m_gpuMemOffsetRangeEnd   = m_gpuMemOffsetRangeStart + m_gpuMemSize;

    for (uint32_t deviceIdx = 0; deviceIdx < m_numPalDevices; deviceIdx++)
    {
        if ((m_gpuMemSize > 0) && (m_internalMem.PalMemory(deviceIdx) != nullptr))
        {
            Pal::Result mapResult = m_internalMem.Map(deviceIdx, &m_pCpuAddr[deviceIdx]);
            VK_ASSERT(mapResult == Pal::Result::Success);

            mapResult = m_internalMem.ShadowMap(deviceIdx, &m_pCpuShadowAddr[deviceIdx]);
            VK_ASSERT(mapResult == Pal::Result::Success);
        }
        else
        {
            m_pCpuShadowAddr[deviceIdx] = nullptr;
            m_pCpuAddr[deviceIdx]       = nullptr;
        }
    }
    Reset();

    return result;
}

// =====================================================================================================================
// Frees the memory for an individual descriptor set.
void DescriptorGpuMemHeap::FreeSetGpuMem(
    void*        pSetAllocHandle)
{
    if (pSetAllocHandle != nullptr)
    {
        DynamicAllocBlock* pBlock = reinterpret_cast<DynamicAllocBlock*>(pSetAllocHandle);

        // At this point this block should not be on the free list.
        VK_ASSERT((pBlock->pPrevFree == nullptr) && (pBlock->pNextFree == nullptr));

        // The deallocation process is as follows:
        //   1. If the next block is free then:
        //      a. Merge the range of the block into the next block
        //      b. Unlink the block from the list and release it
        //      c. Continue as if the next block was the original block
        //   2. If the previous block is free then:
        //      a. If this block is on the free list then unlink the block from the it
        //      b. Merge the range of the block into the previous block
        //      c. Unlink the block from the list and release it
        //   3. If we didn't release the block earlier then it means neither the previous or the next block was
        //      free, thus we should simply link this block to the list of free blocks (it doesn't matter where
        //      we link the block in the free list as that doesn't have to be necessarily ordered)

        bool blockReleased = false;

        // If the next block is a free one then attach the range of this block to it.
        if (IsDynamicAllocBlockFree(pBlock->pNext))
        {
            VK_ASSERT(pBlock->gpuMemOffsetRangeEnd == pBlock->pNext->gpuMemOffsetRangeStart);

            DynamicAllocBlock* pNextBlock = pBlock->pNext;

            // Merge the range of the block into the next block.
            pBlock->pNext->gpuMemOffsetRangeStart = pBlock->gpuMemOffsetRangeStart;

            // Unlink the block from the list.
            pBlock->pNext->pPrev = pBlock->pPrev;
            if (pBlock->pPrev != nullptr)
            {
                pBlock->pPrev->pNext = pBlock->pNext;
            }

            // Then release the block.
            m_pDynamicAllocBlockIndexStack[m_dynamicAllocBlockIndexStackCount++] = DynamicAllocBlockIndex(pBlock);
            blockReleased = true;

            // Set the next block as the block.
            pBlock = pNextBlock;
        }

        // If the previous block is a free one then attach the range of this block to it.
        if (IsDynamicAllocBlockFree(pBlock->pPrev))
        {
            VK_ASSERT(pBlock->gpuMemOffsetRangeStart == pBlock->pPrev->gpuMemOffsetRangeEnd);

            // If this block is on the free list then unlink the block from it.
            if (pBlock->pPrevFree != nullptr)
            {
                pBlock->pPrevFree->pNextFree = pBlock->pNextFree;
            }
            if (pBlock->pNextFree != nullptr)
            {
                pBlock->pNextFree->pPrevFree = pBlock->pPrevFree;
            }

            // Merge the range of the block into the previous block.
            pBlock->pPrev->gpuMemOffsetRangeEnd = pBlock->gpuMemOffsetRangeEnd;

            // Unlink the block from the list.
            pBlock->pPrev->pNext = pBlock->pNext;
            if (pBlock->pNext != nullptr)
            {
                pBlock->pNext->pPrev = pBlock->pPrev;
            }

            // Then release the block.
            m_pDynamicAllocBlockIndexStack[m_dynamicAllocBlockIndexStackCount++] = DynamicAllocBlockIndex(pBlock);
            blockReleased = true;
        }

        // If we didn't release the block so far then let's just link it to the list of free blocks
        if (!blockReleased)
        {
            pBlock->pNextFree = m_dynamicAllocBlockFreeListHeader.pNextFree;

            if (pBlock->pNextFree != nullptr)
            {
                pBlock->pNextFree->pPrevFree = pBlock;
            }

            pBlock->pPrevFree = &m_dynamicAllocBlockFreeListHeader;

            m_dynamicAllocBlockFreeListHeader.pNextFree = pBlock;
        }

#if DEBUG
        // Sanity check the lists after a successful destroy.
        SanityCheckDynamicAllocBlockList();
#endif
    }
}

// =====================================================================================================================
// Frees the memory of all allocations from this heap.
void DescriptorGpuMemHeap::Reset()
{
    bool oneShot = (m_usage & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == 0;

    if (oneShot)
    {
        // Simply reset the forward allocation pointer
        m_oneShotAllocForward = 0;
    }
    else
    {
        VK_ASSERT(m_pDynamicAllocBlocks != nullptr);
        VK_ASSERT(m_pDynamicAllocBlockIndexStack != nullptr);

        // For dynamic allocations the only thing we have to do is release all blocks by resetting the free index stack
        // and then reinitializing the free block list with a single entry covering the entire range.

        m_dynamicAllocBlockIndexStackCount = m_dynamicAllocBlockCount;

        for (uint32_t i = 0; i < m_dynamicAllocBlockIndexStackCount; ++i)
        {
            m_pDynamicAllocBlockIndexStack[i] = i;
        }

        uint32_t blockIndex = m_pDynamicAllocBlockIndexStack[--m_dynamicAllocBlockIndexStackCount];

        DynamicAllocBlock* pBlock      = &m_pDynamicAllocBlocks[blockIndex];
        pBlock->pPrevFree              = &m_dynamicAllocBlockFreeListHeader;
        pBlock->pNextFree              = nullptr;
        pBlock->pPrev                  = nullptr;
        pBlock->pNext                  = nullptr;
        pBlock->gpuMemOffsetRangeStart = m_gpuMemOffsetRangeStart;
        pBlock->gpuMemOffsetRangeEnd   = m_gpuMemOffsetRangeEnd;

        m_dynamicAllocBlockFreeListHeader.pNextFree = pBlock;
    }
}

// =====================================================================================================================
DescriptorSetHeap::DescriptorSetHeap() :
m_nextFreeHandle(0),
m_maxSets(0),
m_pFreeIndexStack(nullptr),
m_freeIndexStackCount(0),
m_pSetMemory(nullptr)
{

}

// =====================================================================================================================
template <uint32_t numPalDevices>
VkResult DescriptorSetHeap::Init(
    Device*                         pDevice,
    VkDescriptorPoolCreateFlags     poolUsage,
    uint32_t                        maxSets)
{
    // Pre-initialize all set memory.  This needs to be done for future purposes because those sets need to all share
    // the same common base array, and the complexity of allocating them in lazy blocks is probably not worth the
    // effort like it is for GPU memory.
    m_maxSets = maxSets;

    // Allocate memory for all sets
    size_t setSize = SetSize<numPalDevices>();

    bool oneShot = (poolUsage & VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) == 0;

    m_pSetMemory = pDevice->VkInstance()->AllocMem(
        maxSets * setSize,
        PAL_CACHE_LINE_BYTES,
        VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

    if (m_pSetMemory == nullptr)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Allocate memory for the free index stack
    if (oneShot == false) //dynamic usage
    {
        m_pFreeIndexStack = reinterpret_cast<uint32_t*>(pDevice->VkInstance()->AllocMem(
            sizeof(uint32_t) * maxSets,
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT));

        if (m_pFreeIndexStack == nullptr)
        {
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Initialize all sets
    for (uint32_t index = 0; index < maxSets; ++index)
    {
        void* pSetMem = Util::VoidPtrInc(m_pSetMemory, index * setSize);

        VK_PLACEMENT_NEW (pSetMem) DescriptorSet<numPalDevices>(index);
    }

    return VK_SUCCESS;
}

// =====================================================================================================================
// Destroys a DescriptorSetHeap
void DescriptorSetHeap::Destroy(
    Device* pDevice)
{
    pDevice->VkInstance()->FreeMem(m_pSetMemory);
    pDevice->VkInstance()->FreeMem(m_pFreeIndexStack);
}

// =====================================================================================================================
// Compute a descriptor set handle from an index in the heap
template <uint32_t numPalDevices>
VkDescriptorSet DescriptorSetHeap::DescriptorSetHandleFromIndex(
    uint32_t idx) const
{
    void* pMem = Util::VoidPtrInc(m_pSetMemory, (SetSize<numPalDevices>() * idx));

    return DescriptorSet<numPalDevices>::HandleFromVoidPointer(pMem);
}

// =====================================================================================================================
// Allocates a new VkDescriptorSet instance and returns a handle to it.
template <uint32_t numPalDevices>
bool DescriptorSetHeap::AllocSetState(
    VkDescriptorSet* pSet)
{
    // First try to allocate through free range start index since it is by far fastest
    if (m_nextFreeHandle < m_maxSets)
    {
        *pSet = DescriptorSetHandleFromIndex<numPalDevices>(m_nextFreeHandle++);

        return true;
    }

    // Otherwise, if we have a free index stack, look there to see if we can pop a free descriptor set
    if (m_freeIndexStackCount > 0)
    {
        --m_freeIndexStackCount;

        *pSet = DescriptorSetHandleFromIndex<numPalDevices>(m_pFreeIndexStack[m_freeIndexStackCount]);

        return true;
    }

    // Otherwise, we are out of luck
    return false;
}

// =====================================================================================================================
// Frees a Vulkan descriptor set instance
template <uint32_t numPalDevices>
void DescriptorSetHeap::FreeSetState(
    VkDescriptorSet set)
{
    // Only care if we have created space for a free index stack
    if (m_pFreeIndexStack != nullptr)
    {
        DescriptorSet<numPalDevices>* pSet = DescriptorSet<numPalDevices>::StateFromHandle(set);

        // We can compute this, but a divide might be a bad idea.
        uint32_t heapIndex = pSet->HeapIndex();

        VK_ASSERT(heapIndex < m_maxSets);

#if DEBUG
        // Clear the descriptor set state for debugging purposes
        pSet->Reset();
#endif

        m_pFreeIndexStack[m_freeIndexStackCount++] = heapIndex;
    }
}

// =====================================================================================================================
// Frees all descriptor set instances.
template <uint32_t numPalDevices>
void DescriptorSetHeap::Reset()
{
    // Reset the next free index to the start of all handles
    m_nextFreeHandle = 0;

    // Clear the individual heap since we've made the whole set range free
    m_freeIndexStackCount = 0;

#if DEBUG
    // Clear the descriptor set states for debugging purposes
    size_t setSize = SetSize<numPalDevices>();

    for (uint32_t index = 0; index < m_maxSets; ++index)
    {
        VkDescriptorSet setHandle =
            DescriptorSet<numPalDevices>::HandleFromVoidPointer(Util::VoidPtrInc(m_pSetMemory, index * setSize));

        DescriptorSet<numPalDevices>::ObjectFromHandle(setHandle)->Reset();
    }
#endif
}

// =====================================================================================================================
template <uint32_t numPalDevices>
VKAPI_ATTR VkResult VKAPI_CALL DescriptorPool::CreateDescriptorPool(
    VkDevice                                    device,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorPool*                           pDescriptorPool)
{
    Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
    const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

    return DescriptorPool::Create<numPalDevices>(pDevice, pCreateInfo, pAllocCB, pDescriptorPool);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
VKAPI_ATTR VkResult VKAPI_CALL DescriptorPool::FreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets)
{
    return DescriptorPool::ObjectFromHandle(descriptorPool)->FreeDescriptorSets<numPalDevices>(
        descriptorSetCount,
        pDescriptorSets);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
VKAPI_ATTR VkResult VKAPI_CALL DescriptorPool::ResetDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorPoolResetFlags                  flags)
{
    return DescriptorPool::ObjectFromHandle(descriptorPool)->Reset<numPalDevices>();
}

// =====================================================================================================================
template <uint32_t numPalDevices>
VKAPI_ATTR VkResult VKAPI_CALL DescriptorPool::AllocateDescriptorSets(
    VkDevice                                    device,
    const VkDescriptorSetAllocateInfo*          pAllocateInfo,
    VkDescriptorSet*                            pDescriptorSets)
{
    return DescriptorPool::ObjectFromHandle(pAllocateInfo->descriptorPool)->AllocDescriptorSets<numPalDevices>(
        pAllocateInfo,
        pDescriptorSets);
}

// =====================================================================================================================
 PFN_vkCreateDescriptorPool DescriptorPool::GetCreateDescriptorPoolFunc(
    Device* pDevice)
{
    PFN_vkCreateDescriptorPool pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
    case 1:
        pFunc = CreateDescriptorPool<1>;
        break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
    case 2:
        pFunc = CreateDescriptorPool<2>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
    case 3:
        pFunc = CreateDescriptorPool<3>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
    case 4:
        pFunc = CreateDescriptorPool<4>;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        break;
    }

    return pFunc;
}

// =====================================================================================================================
PFN_vkFreeDescriptorSets DescriptorPool::GetFreeDescriptorSetsFunc(
    Device* pDevice)
{
    PFN_vkFreeDescriptorSets pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
    case 1:
        pFunc = FreeDescriptorSets<1>;
        break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
    case 2:
        pFunc = FreeDescriptorSets<2>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
    case 3:
        pFunc = FreeDescriptorSets<3>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
    case 4:
        pFunc = FreeDescriptorSets<4>;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        break;
    }

    return pFunc;
}

// =====================================================================================================================
PFN_vkResetDescriptorPool DescriptorPool::GetResetDescriptorPoolFunc(
    Device* pDevice)
{
    PFN_vkResetDescriptorPool pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
    case 1:
        pFunc = ResetDescriptorPool<1>;
        break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
    case 2:
        pFunc = ResetDescriptorPool<2>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
    case 3:
        pFunc = ResetDescriptorPool<3>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
    case 4:
        pFunc = ResetDescriptorPool<4>;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        break;
    }

    return pFunc;
}

// =====================================================================================================================
PFN_vkAllocateDescriptorSets DescriptorPool::GetAllocateDescriptorSetsFunc(
    Device* pDevice)
{
    PFN_vkAllocateDescriptorSets pFunc = nullptr;

    switch (pDevice->NumPalDevices())
    {
    case 1:
        pFunc = AllocateDescriptorSets<1>;
        break;
#if (VKI_BUILD_MAX_NUM_GPUS > 1)
    case 2:
        pFunc = AllocateDescriptorSets<2>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 2)
    case 3:
        pFunc = AllocateDescriptorSets<3>;
        break;
#endif
#if (VKI_BUILD_MAX_NUM_GPUS > 3)
    case 4:
        pFunc = AllocateDescriptorSets<4>;
        break;
#endif
    default:
        VK_NEVER_CALLED();
        break;
    }

    return pFunc;
}

namespace entry
{
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(
    VkDevice                                    device,
    const VkDescriptorPoolCreateInfo*           pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDescriptorPool*                           pDescriptorPool)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->GetEntryPoints().vkCreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    uint32_t                                    descriptorSetCount,
    const VkDescriptorSet*                      pDescriptorSets)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->GetEntryPoints().vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

VKAPI_ATTR VkResult VKAPI_CALL vkResetDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    VkDescriptorPoolResetFlags                  flags)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->GetEntryPoints().vkResetDescriptorPool(device, descriptorPool, flags);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice                                    device,
    VkDescriptorPool                            descriptorPool,
    const VkAllocationCallbacks*                pAllocator)
{
    if (descriptorPool != VK_NULL_HANDLE)
    {
        Device*                      pDevice  = ApiDevice::ObjectFromHandle(device);
        const VkAllocationCallbacks* pAllocCB = pAllocator ? pAllocator : pDevice->VkInstance()->GetAllocCallbacks();

        DescriptorPool::ObjectFromHandle(descriptorPool)->Destroy(pDevice, pAllocCB);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
    VkDevice                                    device,
    const VkDescriptorSetAllocateInfo*          pAllocateInfo,
    VkDescriptorSet*                            pDescriptorSets)
{
    const Device* pDevice = ApiDevice::ObjectFromHandle(device);

    return pDevice->GetEntryPoints().vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

} // namespace entry

} // namespace vk
