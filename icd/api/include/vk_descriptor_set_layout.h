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
 * @file  vk_descriptor_set_layout.h
 * @brief Functionality related to Vulkan descriptor set layout objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_SET_LAYOUT_H__
#define __VK_DESCRIPTOR_SET_LAYOUT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"

namespace vk
{

class Device;

// =====================================================================================================================
// API implementation of Vulkan descriptor set layout objects
//
// Descriptor set layouts define the memory layout of a set of descriptors, as well as how their location in memory
// relates to declared shader resources.
class DescriptorSetLayout : public NonDispatchable<VkDescriptorSetLayout, DescriptorSetLayout>
{
public:
    // Information about a specific section of a descriptor binding
    struct BindingSectionInfo
    {
        uint32_t        dwOffset;      // Dword offset from the start of the set to the binding array
        uint32_t        dwArrayStride; // Array stride between elements in DW
        uint32_t        dwSize;        // Total binding array size in DW
    };

    // Information about an individual binding within this layout
    struct BindingInfo
    {
        VkDescriptorSetLayoutBinding info;  // Vulkan binding information
        BindingSectionInfo  sta;            // Information specific to the static section of the descriptor binding
        BindingSectionInfo  dyn;            // Information specific to the dynamic section of the descriptor binding
        BindingSectionInfo  imm;            // Information specific to the immutable section of the descriptor binding
    };

    // Information about a specific section of a descriptor set layout
    struct SectionInfo
    {
        uint32_t    dwSize;                 // The total number of dwords of this section of one descriptor set
        uint32_t    numRsrcMapNodes;        // Number of required ResourceMappingNodes to build a descriptor
                                            // mapping for this section of the layout during pipeline construction
    };

    // Information about the immutable section of a descriptor set layout
    struct ImmSectionInfo
    {
        uint32_t    numDescriptorValueNodes; // The total number descriptor value node in this layout
        uint32_t    numImmutableSamplers;    // The total number of immutable samplers in the layout
        uint32_t*   pImmutableSamplerData;   // Pointer to the immutable sampler data
    };

    // Set-wide information about this layout
    struct CreateInfo
    {
        uint32_t        count;              // Total number of layout entries
        uint32_t        activeStageMask;    // Shader stage mask describing which stages in which at least one
                                            // binding of this layout's set is active
        uint32_t        numDynamicDescriptors; // Number of dynamic descriptors in this layout
        SectionInfo     sta;                // Information specific to the static section of the descriptor set layout
        SectionInfo     dyn;                // Information specific to the dynamic section of the descriptor set layout
        ImmSectionInfo  imm;                // Information specific to the immutable section of the descriptor set
                                            // layout
        uint32_t        varDescDwStride;    // DWord Size of a descriptor of the type specified for
                                            // the VARIABLE_DESCRIPTOR_COUNT_BIT binding
    };

    static VkResult Create(
        Device*                                     pDevice,
        const VkDescriptorSetLayoutCreateInfo*      pCreateInfo,
        const VkAllocationCallbacks*                pAllocator,
        VkDescriptorSetLayout*                      pLayout);

    void Copy(
        const Device*                               pDevice,
        DescriptorSetLayout*                        pOutLayout) const;

    VkResult Destroy(
        Device*                                     pDevice,
        const VkAllocationCallbacks*                pAllocator,
        bool                                        freeMemory);

    uint32_t GetBindingInfoArrayByteSize() const
    {
        return m_info.count * sizeof(DescriptorSetLayout::BindingInfo);
    }

    uint32_t GetImmSamplerArrayByteSize() const;

    uint32_t GetObjectSize() const
    {
        const uint32_t apiSize = sizeof(DescriptorSetLayout);

        return apiSize + GetBindingInfoArrayByteSize() + GetImmSamplerArrayByteSize();
    }

    const BindingInfo& Binding(uint32_t bindingIndex) const
    {
        // The bindings are allocated immediately after the object.  See DescriptorSetLayout::Create().
        const BindingInfo* pBindings = static_cast<const BindingInfo*>(Util::VoidPtrInc(this, sizeof(*this)));
        return pBindings[bindingIndex];
    }

    const CreateInfo& Info() const { return m_info; }

    const Device* VkDevice() const { return m_pDevice; }

    static uint32_t GetDescStaticSectionDwSize(const Device* pDevice, VkDescriptorType type);
    static uint32_t GetDescDynamicSectionDwSize(const Device* pDevice, VkDescriptorType type);
    static uint32_t GetDescImmutableSectionDwSize(const Device* pDevice, VkDescriptorType type);
    static uint32_t GetDynamicBufferDescDwSize(const Device* pDevice);

    size_t GetDstStaOffset(const BindingInfo& dstBinding, uint32_t dstArrayElement) const
    {
        size_t offset = dstBinding.sta.dwOffset + (dstArrayElement * dstBinding.sta.dwArrayStride);

        return offset;
    }

    size_t GetDstDynOffset(const BindingInfo& dstBinding, uint32_t dstArrayElement) const
    {
        size_t offset = dstBinding.dyn.dwOffset + dstArrayElement * dstBinding.dyn.dwArrayStride;

        return offset;
    }

protected:
    DescriptorSetLayout(
        const Device*     pDevice,
        const CreateInfo& info);

    ~DescriptorSetLayout()
        { }

    static VkResult ConvertCreateInfo(
        const Device*                                pDevice,
        const VkDescriptorSetLayoutCreateInfo*       pIn,
        CreateInfo*                                  pInfo,
        BindingInfo*                                 pOutBindings);

    static void ConvertBindingInfo(
        const VkDescriptorSetLayoutBinding* pBindingInfo,
        uint32_t                            descSizeInDw,
        uint32_t                            descAlignmentInDw,
        SectionInfo*                        pSectionInfo,
        BindingSectionInfo*                 pBindingSectionInfo);

    static void ConvertVariableInfo(
        const VkDescriptorSetLayoutBinding* pBindingInfo,
        uint32_t                            descStaAlignmentInDw,
        uint32_t                            descAlignmentInDw,
        uint32_t&                           varDescDwStride,
        SectionInfo*                        pSectionInfo,
        BindingSectionInfo*                 pBindingSectionInfoVar);

    static void ConvertImmutableInfo(
        const VkDescriptorSetLayoutBinding* pBindingInfo,
        uint32_t                            descSizeInDw,
        ImmSectionInfo*                     pSectionInfo,
        BindingSectionInfo*                 pBindingSectionInfo);

    const CreateInfo          m_info;    // Create-time information
    const Device* const       m_pDevice; // Device pointer
};

namespace entry
{
VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(
    VkDevice                                    device,
    VkDescriptorSetLayout                       descriptorSetLayout,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_DESCRIPTOR_SET_LAYOUT_H__ */
