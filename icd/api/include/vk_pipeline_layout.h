/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  vk_pipeline_layout.h
 * @brief Functionality related to Vulkan pipeline layout objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_PIPELINE_LAYOUT_H__
#define __VK_PIPELINE_LAYOUT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_shader_code.h"

#include "palPipeline.h"

namespace vk
{

class DescriptorSetLayout;

class ShaderModule;

// =====================================================================================================================
// API implementation of Vulkan pipeline layout objects.
//
// Pipeline layout objects provide composite information of all descriptor set layouts across all pipeline
// stages, and how the user-data registers are managed (e.g. single-level table, two-level table, immediate user
// data, etc.).
//
// They are used during pipeline object construction to build layout data for the shader compiler, and during
// CmdBindDescriptorSets to determine how to bind a particular descriptor set to a location within the layout.
class PipelineLayout final : public NonDispatchable<VkPipelineLayout, PipelineLayout>
{
public:
    // Number of user data registers consumed per descriptor set address (we use 32-bit addresses)
    static constexpr uint32_t SetPtrRegCount = 1;

    // Number of user data registers consumed per dynamic descriptor (we supply whole buffer SRDs at the moment)
    // NOTE: This should be changed once we have proper support for dynamic descriptors in SC
    static constexpr uint32_t DynDescRegCount = 4;

    // Magic number describing an invalid or unmapped user data entry
    static constexpr uint8 InvalidReg = UINT8_MAX;

    static constexpr size_t GetMaxResMappingRootNodeSize();
    static constexpr size_t GetMaxResMappingNodeSize();
    static constexpr size_t GetMaxStaticDescValueSize();

    // Set-specific user data layout information
    struct SetUserDataLayout
    {
        // The user data register offsets in this structure are relative to the setBindingRegBase field of
        // the below top-level UserDataLayout.

        uint8    setPtrRegOffset;        // User data register offset to use for this set's set pointer
        uint8    dynDescDataRegOffset;   // User data register offset for this set's dynamic descriptor data
        uint8    dynDescCount;           // Number of dynamic descriptors defined by the descriptor set layout
        uint8    firstRegOffset;         // First user data register offset used by this set layout
        uint8    totalRegCount;          // Total number of user data registers used by this set layout
    };

    // This structure holds information about the user data register allocation scheme of this pipeline layout
    struct Info
    {
        // Top-level user data layout information
        UserDataLayout             userDataLayout;
        // Number of descriptor set bindings in this pipeline layout
        uint32_t                   setCount;
        // Total number of user data registers used in this pipeline layout
        uint32_t                   userDataRegCount;

    };

    // This information is specific for pipeline construction:
    struct PipelineInfo
    {
        // The amount of buffer space needed in the mapping buffer.
        size_t              mappingBufferSize;
        // Max. number of ResourceMappingNodes needed by all layouts in the chain, including the extra nodes
        // required by the extra set pointers, and any resource nodes required by potential internal tables.
        uint32_t            numRsrcMapNodes;
        // Number of resource mapping nodes used for the user data nodes
        uint32_t            numUserDataNodes;
        // Number of DescriptorRangeValue needed by all layouts in the chain
        uint32_t            numDescRangeValueNodes;
    };

    typedef VkPipelineLayout ApiType;

    VkResult BuildLlpcPipelineMapping(
        const uint32_t             stageMask,
        VbBindingInfo*             pVbInfo,
        void*                      pBuffer,
        bool                       appendFetchShaderCb,
        Vkgc::ResourceMappingData* pResourceMapping) const;

    static VkResult Create(
        const Device*                       pDevice,
        const VkPipelineLayoutCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkPipelineLayout*                   pPipelineLayout);

    static VkResult Create(
        const Device*                pDevice,
        const VkPipelineLayout*      pReference,
        const VkShaderStageFlags*    pRefShaderMask,
        const uint32_t               refCount,
        const VkAllocationCallbacks* pAllocator,
        VkPipelineLayout*            pPipelineLayout);

    VkResult Destroy(
        Device*                             pDevice,
        const VkAllocationCallbacks*        pAllocator);

    uint64_t GetApiHash() const
        { return m_apiHash; }

    const PipelineInfo* GetPipelineInfo() const
        { return &m_pipelineInfo; }

    const Info& GetInfo() const
        { return m_info; }

    // Descriptor set layouts in this pipeline layout
    const SetUserDataLayout& GetSetUserData(uint32_t setIndex) const
    {
        return static_cast<const SetUserDataLayout*>(Util::VoidPtrInc(this, sizeof(*this)))[setIndex];
    }

    // Original descriptor set layout pointers
    const DescriptorSetLayout* GetSetLayouts(uint32_t setIndex) const
    {
        return static_cast<const DescriptorSetLayout* const*>(
            Util::VoidPtrInc(this, sizeof(*this) + SetUserDataLayoutSize()))[setIndex];
    }

    DescriptorSetLayout* GetSetLayouts(uint32_t setIndex)
    {
        return static_cast<DescriptorSetLayout**>(
            Util::VoidPtrInc(this, sizeof(*this) + SetUserDataLayoutSize()))[setIndex];
    }

protected:
    static VkResult ConvertCreateInfo(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn,
        Info*                             pInfo,
        PipelineInfo*                     pPipelineInfo,
        SetUserDataLayout*                pSetUserDataLayouts);

    PipelineLayout(
        const Device*       pDevice,
        const Info&         info,
        const PipelineInfo& pipelineInfo,
        uint64_t            apiHash);

    ~PipelineLayout() { }

    VkResult BuildLlpcSetMapping(
        uint32_t                       visibility,
        uint32_t                       setIndex,
        const DescriptorSetLayout*     pLayout,
        Vkgc::ResourceMappingRootNode* pStaNodes,
        uint32_t*                      pStaNodeCount,
        Vkgc::ResourceMappingNode*     pDynNodes,
        uint32_t*                      pDynNodeCount,
        Vkgc::StaticDescriptorValue*   pDescriptorRangeValue,
        uint32_t*                      pDescriptorRangeCount,
        uint32_t                       userDataRegBase) const;

    static uint64_t BuildApiHash(
        const VkPipelineLayoutCreateInfo* pCreateInfo);

    static Vkgc::ResourceMappingNodeType MapLlpcResourceNodeType(
        VkDescriptorType descriptorType);

    static size_t ExtraDataAlignment()
    {
        return Util::Max(alignof(SetUserDataLayout), alignof(DescriptorSetLayout*));
    }

    size_t SetUserDataLayoutSize() const
    {
        return Util::Pow2Align((m_info.setCount * sizeof(SetUserDataLayout)), ExtraDataAlignment());
    }

    const Info              m_info;
    const PipelineInfo      m_pipelineInfo;
    const Device* const     m_pDevice;
    const uint64_t          m_apiHash;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineLayout);
};

static_assert(alignof(PipelineLayout::SetUserDataLayout) <= alignof(PipelineLayout),
    "PipelineLayout::SetUserDataLayout must not have greater alignment than PipelineLayout object!");
static_assert(alignof(DescriptorSetLayout*) <= alignof(PipelineLayout),
    "DescriptorSetLayout* must not have greater alignment than PipelineLayout object!");

constexpr uint32 MaxDescSetRegCount   = MaxDescriptorSets * PipelineLayout::SetPtrRegCount;
constexpr uint32 MaxDynDescRegCount   = MaxDynamicDescriptors * PipelineLayout::DynDescRegCount;
constexpr uint32 MaxBindingRegCount   = MaxDescSetRegCount + MaxDynDescRegCount;
constexpr uint32 MaxPushConstRegCount = MaxPushConstants / 4;

static_assert(PipelineLayout::InvalidReg > (MaxPushConstRegCount + MaxBindingRegCount),
    "PipelineLayout::InvalidReg must be greater than max registers needed.");

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_PIPELINE_LAYOUT_H__ */
