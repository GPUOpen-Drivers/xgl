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
 * @file  vk_descriptor_set.h
 * @brief Functionality related to Vulkan descriptor set objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_SET_H__
#define __VK_DESCRIPTOR_SET_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"
#include "include/vk_utils.h"
#include "include/vk_descriptor_set_layout.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_device.h"

#include "pal.h"

namespace vk
{

class Device;
class DescriptorPool;
class BufferView;

// Flags for the descriptor set.
union DescriptorSetFlags
{
    struct
    {
        uint32_t fmaskBasedMsaaReadEnabled: 1;  // Cached value of associated RuntimeSetting.
        uint32_t robustBufferAccess       : 1;  // Cached value of whether robust buffer access is used
        uint32_t reserved                 : 30;
    };
    uint32_t u32All;
};

// =====================================================================================================================
// A descriptor set is a chunk of GPU memory containing one or more descriptors organized in a manner described by a
// DescriptorSetLayout associated with it.  They are allocated and freed by DescriptorPools.
class DescriptorSet : public NonDispatchable<VkDescriptorSet, DescriptorSet>
{
public:
    VK_INLINE void WriteSamplerDescriptors(
        const Device::Properties&       deviceProperties,
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    VK_INLINE void WriteImageSamplerDescriptors(
        const Device::Properties&       deviceProperties,
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    VK_INLINE void WriteImageDescriptors(
        VkDescriptorType                descType,
        const Device::Properties&       deviceProperties,
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    VK_INLINE void WriteFmaskDescriptors(
        const Device*                   pDevice,
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    VK_INLINE void WriteBufferInfoDescriptors(
        const Device*                   pDevice,
        VkDescriptorType                type,
        const VkDescriptorBufferInfo*   pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    VK_INLINE void WriteBufferDescriptors(
        const Device::Properties&       deviceProperties,
        VkDescriptorType                type,
        const VkBufferView*             pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes);

    const VK_INLINE bool FmaskBasedMsaaReadEnabled() const
        { return m_flags.fmaskBasedMsaaReadEnabled; }

    const VK_INLINE bool RobustBufferAccess() const
        { return m_flags.robustBufferAccess; }

    const DescriptorSetLayout* Layout() const
        { return m_pLayout; }

    size_t Size() const
        { return (m_pLayout->Info().sta.dwSize + m_pLayout->Info().fmask.dwSize) * sizeof(uint32_t); }

    Pal::gpusize GpuAddress(int32_t idx) const
    {
        return m_gpuAddress[idx];
    }

    uint32_t* CpuAddress(int32_t idx) const
    {
        return m_pCpuAddress[idx];
    }

    uint32_t* DynamicDescriptorData()
        { return m_dynamicDescriptorData; }

    VkResult Destroy(Device* pDevice);

    VK_INLINE static DescriptorSet* StateFromHandle(VkDescriptorSet set);
    VK_INLINE static Pal::gpusize GpuAddressFromHandle(uint32_t deviceIdx, VkDescriptorSet set);
    VK_INLINE static void UserDataPtrValueFromHandle(VkDescriptorSet set, uint32_t deviceIdx, uint32_t* pUserData);

    VK_INLINE static void PatchedDynamicDataFromHandle(
        VkDescriptorSet set,
        uint32_t*       pUserData,
        const uint32_t* pDynamicOffsets,
        uint32_t        numDynamicDescriptors);

    static void WriteDescriptorSets(
        const Device*                pDevice,
        uint32_t                     deviceIdx,
        const Device::Properties&    deviceProperties,
        uint32_t                     descriptorWriteCount,
        const VkWriteDescriptorSet*  pDescriptorWrites,
        size_t                       descriptorStrideInBytes = 0);

    static void CopyDescriptorSets(
        const Device*                pDevice,
        uint32_t                     deviceIdx,
        const Device::Properties&    deviceProperties,
        uint32_t                     descriptorCopyCount,
        const VkCopyDescriptorSet*   pDescriptorCopies);

protected:
    DescriptorSet(
        DescriptorPool*    pPool,
        uint32_t           heapIndex,
        DescriptorSetFlags flags);

    ~DescriptorSet()
        { PAL_NEVER_CALLED(); }

    void Reassign(
        const DescriptorSetLayout*  pLayout,
        Pal::gpusize                gpuMemOffset,
        Pal::gpusize*               gpuBaseAddress,
        uint32_t**                  cpuBaseAddress,
        uint32_t                    numPalDevices,
        const InternalMemory* const pInternalMem,
        void*                       pAllocHandle,
        VkDescriptorSet*            pHandle);

    void InitImmutableDescriptors(
        const DescriptorSetLayout*  pLayout,
        uint32_t                    numPalDevices);

    void* AllocHandle() const
        { return m_pAllocHandle; }

    uint32_t HeapIndex() const
        { return m_heapIndex; }

    const DescriptorSetLayout*  m_pLayout;
    void*                       m_pAllocHandle;
    Pal::gpusize                m_gpuAddress[MaxPalDevices];
    uint32_t*                   m_pCpuAddress[MaxPalDevices];

    DescriptorPool* const       m_pPool;
    uint32_t                    m_heapIndex;

    const DescriptorSetFlags    m_flags;

    // NOTE: This is hopefully only needed temporarily until SC implements proper support for buffer descriptors
    // with dynamic offsets. Until then we have to store the static portion of dynamic buffer descriptors in client
    // memory together with the descriptor set so that we are able to supply the patched version of the descriptors.
    uint32_t                    m_dynamicDescriptorData[MaxDynamicDescriptors * PipelineLayout::DynDescRegCount];

    friend class DescriptorPool;
    friend class DescriptorSetHeap;
};

// =====================================================================================================================
// Returns the full driver state pointer of an VkDescriptorSet
DescriptorSet* DescriptorSet::StateFromHandle(
    VkDescriptorSet set)
{
    return DescriptorSet::ObjectFromHandle(set);
}

// =====================================================================================================================
// Returns the GPU VA of a VkDescriptorSet
Pal::gpusize DescriptorSet::GpuAddressFromHandle(
    uint32_t        deviceIdx,
    VkDescriptorSet set)
{
    return StateFromHandle(set)->GpuAddress(deviceIdx);
}

// =====================================================================================================================
void DescriptorSet::UserDataPtrValueFromHandle(
    VkDescriptorSet set,
    uint32_t        deviceIdx,
    uint32_t*       pUserData)
{
    static_assert(PipelineLayout::SetPtrRegCount == 1, "Code below assumes one dword per set GPU VA");

    const Pal::gpusize gpuAddress = GpuAddressFromHandle(deviceIdx, set);

    // We have an assumed high 32 bits for the address thus only the lower 32 bits of the address have to be used here
    *pUserData = static_cast<uint32_t>(gpuAddress & 0xFFFFFFFFull);
}

// =====================================================================================================================
// Returns the patched dynamic descriptor data for the specified descriptor set.
// NOTE: This function assumes that we directly store the whole buffer SRDs in user data and treats the SRD data in a
// white-box fashion. Probably would be better if we'd have a PAL function to do the patching of the dynamic offset,
// but this is expected to be temporary anyways until we'll have proper support for dynamic descriptors in SC.
void DescriptorSet::PatchedDynamicDataFromHandle(
    VkDescriptorSet set,
    uint32_t*       pUserData,
    const uint32_t* pDynamicOffsets,
    uint32_t        numDynamicDescriptors)
{
    // This code expects 4 DW SRDs whose first 48 bits is the base address.

    DescriptorSet* pSet  = StateFromHandle(set);
    uint64_t* pDstQwords = reinterpret_cast<uint64_t*>(pUserData);
    uint64_t* pSrcQwords = reinterpret_cast<uint64_t*>(pSet->DynamicDescriptorData());
    const uint32_t dynDataNumQwords = pSet->RobustBufferAccess() ? 2 : 1;
    for (uint32_t i = 0; i < numDynamicDescriptors; ++i)
    {
        const uint64_t baseAddressMask = 0x0000FFFFFFFFFFFFull;

        // Read default base address
        uint64_t baseAddress = pSrcQwords[i * dynDataNumQwords] & baseAddressMask;
        uint64_t hiBits      = pSrcQwords[i * dynDataNumQwords] & ~baseAddressMask;

        // Add dynamic offset
        baseAddress += pDynamicOffsets[i];

        pDstQwords[i * dynDataNumQwords] = hiBits | baseAddress;

        if (pSet->RobustBufferAccess())
        {
            pDstQwords[i * dynDataNumQwords + 1] = pSrcQwords[i * dynDataNumQwords + 1];
        }
    }
}

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(
    VkDevice                                    device,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet*                 pDescriptorWrites,
    uint32_t                                    descriptorCopyCount,
    const VkCopyDescriptorSet*                  pDescriptorCopies);

}

} // namespace vk

#endif /* __VK_DESCRIPTOR_SET_H__ */
