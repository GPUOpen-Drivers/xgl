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

struct DescriptorAddr
{
    Pal::gpusize  staticGpuAddr;
    uint32_t*     staticCpuAddr;
    uint32_t*     fmaskCpuAddr;
};

// =====================================================================================================================
// A descriptor set is a chunk of GPU memory containing one or more descriptors organized in a manner described by a
// DescriptorSetLayout associated with it.  They are allocated and freed by DescriptorPools.
template <uint32_t numPalDevices>
class DescriptorSet final : public NonDispatchable<VkDescriptorSet, DescriptorSet<numPalDevices>>
{
public:
    DescriptorSet(uint32_t heapIndex);

    void Reassign(
        const DescriptorSetLayout*  pLayout,
        Pal::gpusize                gpuMemOffset,
        DescriptorAddr*             pBaseAddrs,
        void*                       pAllocHandle);

    void WriteImmutableSamplers(
        uint32_t imageDescSizeInBytes);

    const DescriptorSetLayout* Layout() const
        { return m_pLayout; }

    size_t Size() const
        { return (m_pLayout->Info().sta.dwSize * sizeof(uint32_t)); }

    Pal::gpusize StaticGpuAddress(int32_t idx) const
    {
        return m_addresses[idx].staticGpuAddr;
    }

    uint32_t* StaticCpuAddress(int32_t idx) const
    {
        return m_addresses[idx].staticCpuAddr;
    }

    uint32_t* FmaskCpuAddress(int32_t idx) const
    {
        return m_addresses[idx].fmaskCpuAddr;
    }

    uint32_t* DynamicDescriptorData(uint32_t idx)
    {
        return reinterpret_cast<uint32_t*>(&(DynamicDescriptorData()[idx]));
    }

    uint64_t* DynamicDescriptorDataQw(uint32_t idx)
    {
        return &(DynamicDescriptorData()[idx]);
    }

    inline static DescriptorSet* StateFromHandle(VkDescriptorSet set);
    inline static Pal::gpusize GpuAddressFromHandle(uint32_t deviceIdx, VkDescriptorSet set);
    inline static void UserDataPtrValueFromHandle(VkDescriptorSet set, uint32_t deviceIdx, uint32_t* pUserData);

     inline static void PatchedDynamicDataFromHandle(
        VkDescriptorSet set,
        uint32_t        deviceIdx,
        uint32_t*       pUserData,
        const uint32_t* pDynamicOffsets,
        uint32_t        numDynamicDescriptors,
        bool            useCompactDescriptor);

protected:
    ~DescriptorSet()
        { PAL_NEVER_CALLED(); }

    void Reset();

    void* AllocHandle() const
        { return m_pAllocHandle; }

    uint32_t HeapIndex() const
        { return m_heapIndex; }

    uint64* DynamicDescriptorData()
        { return static_cast<uint64*>( Util::VoidPtrInc(this, sizeof(*this))); }

    const DescriptorSetLayout*  m_pLayout;
    void*                       m_pAllocHandle;
    DescriptorAddr              m_addresses[numPalDevices];

    uint32_t                    m_heapIndex;

    friend class DescriptorPool;
    friend class DescriptorSetHeap;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorSet);
};

// =====================================================================================================================
// Returns the full driver state pointer of an VkDescriptorSet
template <uint32_t numPalDevices>
DescriptorSet<numPalDevices>* DescriptorSet<numPalDevices>::StateFromHandle(
    VkDescriptorSet set)
{
    return DescriptorSet<numPalDevices>::ObjectFromHandle(set);
}

// =====================================================================================================================
// Returns the GPU VA of a VkDescriptorSet
template <uint32_t numPalDevices>
Pal::gpusize DescriptorSet<numPalDevices>::GpuAddressFromHandle(
    uint32_t        deviceIdx,
    VkDescriptorSet set)
{
    return StateFromHandle(set)->StaticGpuAddress(deviceIdx);
}

// =====================================================================================================================
template <uint32_t numPalDevices>
void DescriptorSet<numPalDevices>::UserDataPtrValueFromHandle(
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
// NOTE: This function assumes the descriptor format that will be written to user data in a white-box fashion. If the
// format of the buffer address changes for either compact or uncompact descriptors, it needs to be updated here too.
// PAL could provide a query for us to interpret the HW dependency, the implementation of a similar patching function,
// or the descriptors could be written here instead of in UpdateDescriptorSets (though likely with some redundant work
// on binds and poorly-packed intermediate data in the descriptor set).
template <uint32_t numPalDevices>
void DescriptorSet<numPalDevices>::PatchedDynamicDataFromHandle(
    VkDescriptorSet set,
    uint32_t        deviceIdx,
    uint32_t*       pUserData,
    const uint32_t* pDynamicOffsets,
    uint32_t        numDynamicDescriptors,
    bool            useCompactDescriptor)
{
    // This code expects descriptors whose first 48 bits is the base address.
    DescriptorSet<numPalDevices>* pSet = StateFromHandle(set);
    const uint64_t* pSrcQwords         = pSet->DynamicDescriptorDataQw(deviceIdx);
    const uint32_t  dynDataNumQwords   = useCompactDescriptor ? 1 : 2;

    for (uint32_t i = 0; i < numDynamicDescriptors; ++i)
    {
        const uint64_t baseAddressMask = 0x0000FFFFFFFFFFFFull;

        // Read default base address
        uint64_t baseAddress  = pSrcQwords[i * dynDataNumQwords] & baseAddressMask;
        const uint64_t hiBits = pSrcQwords[i * dynDataNumQwords] & ~baseAddressMask;

        // Add dynamic offset
        baseAddress += pDynamicOffsets[i];
        baseAddress = hiBits | baseAddress;

        // We have to use memcpy because we are accessing memory not alignment to 64bit (array uint32_t [1][1]),
        // what causes segmentation fault on linux when we cast pUserData to unit64_t pointer and use it.
        memcpy(&pUserData[2 * i * dynDataNumQwords], &baseAddress, sizeof(uint64_t));

        if (!useCompactDescriptor)
        {
            memcpy(&pUserData[2 * i * dynDataNumQwords + 2], &pSrcQwords[i * dynDataNumQwords + 1], sizeof(uint64_t));
        }
    }
}

// =====================================================================================================================

class DescriptorUpdate
{
public:
    template <size_t samplerDescSize>
    static void WriteSamplerDescriptors(
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t imageDescSize, size_t samplerDescSize>
    static void WriteImageSamplerDescriptors(
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t imageDescSize, bool isShaderStorageDesc>
    static void WriteImageDescriptors(
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t imageDescSize>
    static void WriteImageDescriptorsYcbcr(
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t imageDescSize, size_t fmaskDescSize>
    static void WriteFmaskDescriptors(
        const VkDescriptorImageInfo*    pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t bufferDescSize, VkDescriptorType type>
    static void WriteBufferInfoDescriptors(
        const Device*                   pDevice,
        const VkDescriptorBufferInfo*   pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

    template <size_t bufferDescSize, VkDescriptorType type>
    static void WriteBufferDescriptors(
        const VkBufferView*             pDescriptors,
        uint32_t                        deviceIdx,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride,
        size_t                          descriptorStrideInBytes = 0);

#if VKI_RAY_TRACING
    static void SetAccelerationDescriptorsBufferViewFlags(
        const Device*                       pDevice,
        Pal::BufferViewInfo*                pBufferViewInfo);

    static void WriteAccelerationStructureDescriptors(
        const Device*                       pDevice,
        const VkAccelerationStructureKHR*   pDescriptors,
        uint32_t                            deviceIdx,
        uint32_t*                           pDestAddr,
        uint32_t                            count,
        uint32_t                            dwStride,
        size_t                              descriptorStrideInBytes = 0);
#endif

    static void WriteInlineUniformBlock(
        const void*                     pData,
        uint32_t*                       pDestAddr,
        uint32_t                        count,
        uint32_t                        dwStride);

    static PFN_vkUpdateDescriptorSets GetUpdateDescriptorSetsFunc(const Device* pDevice);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorUpdate);

    template <uint32_t numPalDevices>
    static PFN_vkUpdateDescriptorSets GetUpdateDescriptorSetsFunc(const Device* pDevice);

    template <size_t imageDescSize,
              size_t fmaskDescSize,
              size_t samplerDescSize,
              size_t bufferDescSize,
              uint32_t numPalDevices>
    static VKAPI_ATTR void VKAPI_CALL UpdateDescriptorSets(
        VkDevice                                    device,
        uint32_t                                    descriptorWriteCount,
        const VkWriteDescriptorSet*                 pDescriptorWrites,
        uint32_t                                    descriptorCopyCount,
        const VkCopyDescriptorSet*                  pDescriptorCopies);

    template <size_t imageDescSize,
              size_t fmaskDescSize,
              size_t samplerDescSize,
              size_t bufferDescSize,
              uint32_t numPalDevices>
    static void WriteDescriptorSets(
        const Device*                pDevice,
        uint32_t                     deviceIdx,
        uint32_t                     descriptorWriteCount,
        const VkWriteDescriptorSet*  pDescriptorWrites);

    template <size_t imageDescSize, size_t fmaskDescSize, uint32_t numPalDevices>
    static void CopyDescriptorSets(
        const Device*                pDevice,
        uint32_t                     deviceIdx,
        uint32_t                     descriptorCopyCount,
        const VkCopyDescriptorSet*   pDescriptorCopies);
};

// =====================================================================================================================

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
