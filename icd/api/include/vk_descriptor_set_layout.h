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
 * @file  vk_descriptor_set_layout.h
 * @brief Functionality related to Vulkan descriptor set layout objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_DESCRIPTOR_SET_LAYOUT_H__
#define __VK_DESCRIPTOR_SET_LAYOUT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_dispatch.h"

namespace Util
{

class MetroHash64;

}

namespace vk
{

// Fixed offset for pipeline internal resource.
constexpr uint32_t FetchShaderInternalBufferOffset = 0;
constexpr uint32_t SpecConstBufferVertexOffset     = 2;
constexpr uint32_t SpecConstBufferFragmentOffset   = 4;

// Constants for Angle style descriptor layout pattern
namespace AngleDescPattern
{
constexpr uint32_t DescriptorSetOffset[4]          = { 6, 10, 18, 19 };
constexpr uint32_t DescriptorSetBindingStride      = 12;
}

// Internal descriptor binding flags, which contains mapping of VkDescriptorBindingFlagBits
struct DescriptorBindingFlags
{
    union
    {
        struct
        {
            uint32_t variableDescriptorCount :  1; // Map from VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
            uint32_t ycbcrConversionUsage    :  1; // Indicate a binding contains Ycbcr conversion sampler
            uint32_t reserved                : 30;
        };
        uint32_t u32all;
    };
};

class Device;

// =====================================================================================================================
// API implementation of Vulkan descriptor set layout objects
//
// Descriptor set layouts define the memory layout of a set of descriptors, as well as how their location in memory
// relates to declared shader resources.
class DescriptorSetLayout final : public NonDispatchable<VkDescriptorSetLayout, DescriptorSetLayout>
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
        VkDescriptorSetLayoutBinding   info;         // Vulkan binding information
        DescriptorBindingFlags         bindingFlags; // Binding flags for descriptor binding
        BindingSectionInfo             sta;          // Information specific to the static section of the descriptor binding
        BindingSectionInfo             dyn;          // Information specific to the dynamic section of the descriptor binding
        BindingSectionInfo             imm;          // Information specific to the immutable section of the descriptor binding
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
        uint32_t    numDescriptorValueNodes;   // The total number descriptor value node in this layout
        uint32_t    numImmutableSamplers;      // The total number of immutable samplers in the layout
        uint32_t    numImmutableYCbCrMetaData; // The total number of immutable ycbcr meta data in the layout
        uint32_t*   pImmutableSamplerData;     // Pointer to the immutable sampler data
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
        uint32_t        varDescStride;      // Byte size of a descriptor of the type specified for
                                            // the VARIABLE_DESCRIPTOR_COUNT_BIT binding

        VkDescriptorSetLayoutCreateFlags flags;
    };

    static VkResult Create(
        const Device*                          pDevice,
        const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
        const VkAllocationCallbacks*           pAllocator,
        VkDescriptorSetLayout*                 pLayout);

    static size_t GetObjectSize(
        const VkDescriptorSetLayout* pLayouts,
        const VkShaderStageFlags*    pShaderMasks,
        const uint32_t               count);

    void Copy(
        const Device*        pDevice,
        DescriptorSetLayout* pOutLayout) const;

    VkResult Destroy(
        Device*                      pDevice,
        const VkAllocationCallbacks* pAllocator,
        bool                         freeMemory);

    size_t GetBindingInfoArrayByteSize(VkShaderStageFlags shaderMask) const;

    size_t GetImmSamplerArrayByteSize(VkShaderStageFlags shaderMask) const;

    size_t GetImmYCbCrMetaDataArrayByteSize(VkShaderStageFlags shaderMask) const;

    size_t GetObjectSize(VkShaderStageFlags shaderMask) const;

    bool IsEmpty(VkShaderStageFlags shaderMask) const;

    const BindingInfo& Binding(uint32_t bindingIndex) const
    {
        // The bindings are allocated immediately after the object.  See DescriptorSetLayout::Create().
        const BindingInfo* pBindings = static_cast<const BindingInfo*>(Util::VoidPtrInc(this, sizeof(*this)));
        return pBindings[bindingIndex];
    }

    const CreateInfo& Info() const { return m_info; }

    const Device* VkDevice() const { return m_pDevice; }

    static uint32_t GetSingleDescStaticSize(const Device* pDevice, VkDescriptorType type);

    static uint32_t GetDescStaticSectionDwSize(
        const Device*                       pDevice,
        const VkDescriptorSetLayoutBinding* type,
        const DescriptorBindingFlags        bindingFlags,
        const bool                          useFullYcbrImageSampler);

    static uint32_t GetDescStaticSectionDwSize(
        const DescriptorSetLayout* pSrcDescSetLayout,
        const uint32_t             binding);

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

    uint64_t GetApiHash() const
        { return m_apiHash; }

protected:
    DescriptorSetLayout(
        const Device*     pDevice,
        const CreateInfo& info,
        uint64_t          apiHash);

    ~DescriptorSetLayout()
        { }

    bool CoverAllActiveShaderStages(const uint32_t shaderMask) const
    {
        return ((~shaderMask & m_info.activeStageMask) == 0);
    }

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

    static void ConvertImmutableInfo(
        const VkDescriptorSetLayoutBinding* pBindingInfo,
        uint32_t                            descSizeInDw,
        ImmSectionInfo*                     pSectionInfo,
        BindingSectionInfo*                 pBindingSectionInfo,
        const DescriptorBindingFlags        bindingFlags,
        const DescriptorSetLayout*          pSrcDescSetLayout = nullptr);

    static void GenerateHashFromBinding(
        Util::MetroHash64*                  pHasher,
        const VkDescriptorSetLayoutBinding& desc);

    static uint64_t BuildApiHash(
        const VkDescriptorSetLayoutCreateInfo* pCreateInfo);

    const CreateInfo          m_info;    // Create-time information
    const Device* const       m_pDevice; // Device pointer
    const uint64_t            m_apiHash;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DescriptorSetLayout);
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
