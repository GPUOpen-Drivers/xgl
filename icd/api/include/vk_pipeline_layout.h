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

namespace Llpc
{
struct PipelineShaderInfo;
struct ResourceMappingNode;
struct DescriptorRangeValue;
enum class ResourceMappingNodeType : uint32_t;
};

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
class PipelineLayout : public NonDispatchable<VkPipelineLayout, PipelineLayout>
{

public:
    // Number of user data registers consumed per descriptor set address (we use 32-bit addresses)
    static constexpr uint32_t SetPtrRegCount = 1;

    // Number of user data registers consumed per dynamic descriptor (we supply whole buffer SRDs at the moment)
    // NOTE: This should be changed once we have proper support for dynamic descriptors in SC
    static constexpr uint32_t DynDescRegCount = 4;

    // Magic number describing an invalid or unmapped user data entry
    static constexpr uint32_t InvalidReg = UINT32_MAX;

    // Set-specific user data layout information
    struct SetUserDataLayout
    {
        // The user data register offsets in this structure are relative to the setBindingRegBase field of
        // the below top-level UserDataLayout.

        uint32_t    setPtrRegOffset;        // User data register offset to use for this set's set pointer
        uint32_t    dynDescDataRegOffset;   // User data register offset for this set's dynamic descriptor data
        uint32_t    dynDescDataRegCount;    // Number of registers for the dynamic descriptor data
        uint32_t    dynDescCount;           // Number of dynamic descriptors defined by the descriptor set layout
        uint32_t    firstRegOffset;         // First user data register offset used by this set layout
        uint32_t    totalRegCount;          // Total number of user data registers used by this set layout
    };

    // The top-level user data layout is portioned into different sections based on the value type (push constant,
    // descriptor set addresses, etc.).  This structure describes the offsets and sizes of those regions.
    struct UserDataLayout
    {
        // Base user data register index to use for the descriptor set binding data (including registers for
        // dynamic descriptor offsets)
        uint32_t setBindingRegBase;
        // Number of user data registers used for the set binding points
        uint32_t setBindingRegCount;

        // Base user data register index to use for push constants
        uint32_t pushConstRegBase;
        // Number of user data registers used for push constants
        uint32_t pushConstRegCount;
    };

    // This structure holds information about the user data register allocation scheme of this pipeline layout
    struct Info
    {
        // Top-level user data layout information
        UserDataLayout             userDataLayout;
        // Number of descriptor set bindings in this pipeline layout
        uint32_t                   setCount;
        // Array of descriptor set layouts in this pipeline layout, indexed by set index
        SetUserDataLayout          setUserData[MaxDescriptorSets];
        // Total number of user data registers used in this pipeline layout
        uint32_t                   userDataRegCount;
        // Original descriptor set layout pointers
        const DescriptorSetLayout* pSetLayouts[MaxDescriptorSets];
    };

    // This information is specific for pipeline construction:
    struct PipelineInfo
    {
        // The total amount of buffer space needed in the helper buffer.
        size_t              tempBufferSize;
        // Size in the buffer required per shader stage.
        size_t              tempStageSize;
        // Max. number of Pal::ResourceMappingNodes needed by all layouts in the chain, including the extra nodes
        // required by the extra set pointers, and any resource nodes required by potential internal tables.
        uint32_t            numPalRsrcMapNodes;
        // Number of resource mapping nodes used for the user data nodes
        uint32_t            numUserDataNodes;
        // Number of immutable sampler descriptors
        // Number of Pal::DescriptorRangeValue needed by all layouts in the chain
        uint32_t            numPalDescRangeValueNodes;
        // Number of immutable samplers used for all descriptor range value node
        uint32_t            numImmutableSamplers;
        // Immutable sampler descriptor data pointer
        uint32_t*           pImmutableSamplerData;
    };

    typedef VkPipelineLayout ApiType;

#ifndef VK_OEPN_SOURCE
    VkResult BuildPipelineMapping(
        ShaderStage                                 stage,
        const void*                                 pShaderPatchOut,
        void*                                       pBuffer,
        const VkPipelineVertexInputStateCreateInfo* pVertexInput,
        Pal::PipelineShaderInfo*                    pShaderInfo,
        VbBindingInfo*                              pVbInfo) const;
#endif

    VkResult BuildLlpcPipelineMapping(
        ShaderStage                                 stage,
        void*                                       pBuffer,
        const VkPipelineVertexInputStateCreateInfo* pVertexInput,
        Llpc::PipelineShaderInfo*                   pShaderInfo,
        VbBindingInfo*                              pVbInfo) const;
    static VkResult Create(
        const Device*                       pDevice,
        const VkPipelineLayoutCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkPipelineLayout*                   pPipelineLayout);

    VkResult Destroy(
        Device*                             pDevice,
        const VkAllocationCallbacks*        pAllocator);

    VK_INLINE const PipelineInfo* GetPipelineInfo() const
        { return &m_pipelineInfo; }

    VK_INLINE const Info& GetInfo() const
        { return m_info; }

protected:
    static VkResult ConvertCreateInfo(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn,
        Info*                             pInfo,
        PipelineInfo*                     pPipelineInfo);

    PipelineLayout(
        const Device*       pDevice,
        const Info&         info,
        const PipelineInfo& pipelineInfo);

    ~PipelineLayout() { }
    int32_t  BuildVertexInputDescriptors(
        const void*                                     pShaderPatchOut,
        const VkPipelineVertexInputStateCreateInfo*     pInput,
        Pal::ResourceMappingNode*                       pSetNodes,
        VbBindingInfo*                                  pInfo) const;

    VkResult BuildLlpcSetMapping(
        ShaderStage                  stage,
        uint32_t                     setIndex,
        const DescriptorSetLayout*   pLayout,
        Llpc::ResourceMappingNode*   pStaNodes,
        uint32_t*                    pStaNodeCount,
        Llpc::ResourceMappingNode*   pDynNodes,
        uint32_t*                    pDynNodeCount,
        Llpc::DescriptorRangeValue*  pDescriptorRangeValue,
        uint32_t*                    pDescriptorRangeCount,
        uint32_t                     userDataRegBase) const;

    int32_t BuildLlpcVertexInputDescriptors(
        const VkPipelineVertexInputStateCreateInfo* pInput,
        VbBindingInfo*                              pVbInfo) const;

    static Llpc::ResourceMappingNodeType MapLlpcResourceNodeType(
        VkDescriptorType descriptorType);
    const Info              m_info;
    const PipelineInfo      m_pipelineInfo;
    const Device* const     m_pDevice;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice                                    device,
    VkPipelineLayout                            pipelineLayout,
    const VkAllocationCallbacks*                pAllocator);
} // namespace entry

} // namespace vk

#endif /* __VK_PIPELINE_LAYOUT_H__ */
