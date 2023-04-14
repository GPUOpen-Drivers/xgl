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
 * @file  vk_pipeline_layout.h
 * @brief Functionality related to Vulkan pipeline layout objects.
 ***********************************************************************************************************************
 */

#ifndef __VK_PIPELINE_LAYOUT_H__
#define __VK_PIPELINE_LAYOUT_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_device.h"

namespace vk
{

class DescriptorSetLayout;

// Determine mapping layout of the resouces used in shaders
enum class PipelineLayoutScheme : uint32_t
{
    // Compact scheme makes full use of all the user data registers and can achieve best performance in theory.
    // See PipelineLayout::BuildCompactSchemeInfo() for more details
    Compact = 0,
    // The searching path of resource belongs to a specific binding is fixed in indirect scheme.
    // See PipelineLayout::BuildIndirectSchemeInfo() for more details
    Indirect
};

// The top-level user data layout is portioned into different sections based on the value type (push constant,
// descriptor set addresses, etc.).  This structure describes the offsets and sizes of those regions.
struct UserDataLayout
{
    PipelineLayoutScheme scheme;

    union
    {
        struct
        {
            // Base user data register index to use for the descriptor set binding data
            // (including registers for dynamic descriptor offsets)
            uint32_t setBindingRegBase;
            // Number of user data registers used for the set binding points
            uint32_t setBindingRegCount;

            // Base user data register index to use for push constants
            uint32_t pushConstRegBase;
            // Number of user data registers used for push constants
            uint32_t pushConstRegCount;

            // Base user data register index to use for transform feedback.
            uint32_t transformFeedbackRegBase;
            // Number of user data registers used for transform feedback
            uint32_t transformFeedbackRegCount;

            // Base user data register index to use for the constant buffer used in uber-fetch shader
            // The number of user data register used is always 2
            uint32_t uberFetchConstBufRegBase;

            // Base user data register indices to use for buffers storing specialization constants
            uint32_t specConstBufVertexRegBase;
            uint32_t specConstBufFragmentRegBase;
            // Base use data register for debug printf
            uint32_t debugPrintfRegBase;

#if VKI_RAY_TRACING
            // Base user data register index to use for ray tracing capture replay VA mapping internal buffer
            uint32_t rtCaptureReplayConstBufRegBase;
#endif

            // Base user data register index to use for thread group order reversal state
            uint32_t threadGroupReversalRegBase;

        } compact;

        struct
        {
            // Base user data register index to use for transform feedback.
            // The number of user data register used is always 1
            uint32_t transformFeedbackRegBase;

            // Base user data register index to use for the pointers pointing to the buffers
            // storing descriptor set binding data.
            // Each set occupy 2 entries: one for static and one for dynamic descriptors
            // The total number of user data registers used is always MaxDescriptorSets * 2 * SetPtrRegCount
            uint32_t setBindingPtrRegBase;

            // Base user data register index to use for buffer storing push constant data
            // The number of user data register used is always 1
            uint32_t pushConstPtrRegBase;

            // The size of buffer required to store push constants
            uint32_t pushConstSizeInDword;
            // Base use data register for debug printf
            uint32_t debugPrintfRegBase;

#if VKI_RAY_TRACING
            // Base user data register index to use for buffer storing ray tracing dispatch arguments
            // The number of user data registers used is always 1
            uint32_t dispatchRaysArgsPtrRegBase;
#endif

            // Base user data register index to use for the constant buffer used in uber-fetch shader
            // The number of user data register used is always 2
            uint32_t uberFetchConstBufRegBase;

#if VKI_RAY_TRACING
            // Base user data register index to use for ray tracing capture replay VA mapping internal buffer
            uint32_t rtCaptureReplayConstBufRegBase;
#endif

            // Base user data register index to use for thread group order reversal state
            uint32_t threadGroupReversalRegBase;

        } indirect;
    };
};

// =====================================================================================================================
inline uint32_t GetUberFetchShaderUserData(
    const UserDataLayout* pLayout)
{
    return (pLayout->scheme == PipelineLayoutScheme::Compact) ?
        pLayout->compact.uberFetchConstBufRegBase : pLayout->indirect.uberFetchConstBufRegBase;
}

// =====================================================================================================================
inline void SetUberFetchShaderUserData(
    UserDataLayout* pLayout,
    uint32_t        regBase)
{
    uint32_t* pRegBaseAddr = (pLayout->scheme == PipelineLayoutScheme::Compact) ?
        &pLayout->compact.uberFetchConstBufRegBase : &pLayout->indirect.uberFetchConstBufRegBase;
    *pRegBaseAddr = regBase;
}

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

    // Number of user data registers consumed per dynamic descriptor (compact descriptors only require 2 if used)
    static constexpr uint32_t DynDescRegCount = 4;

    // PAL requires all indirect user data tables to be 1DW
    static constexpr uint32_t VbTablePtrRegCount = 1;

    // DescriptorBufferCompact node, which is used to represent internal constant buffer always requires 2DW
    // user data entries
    static constexpr uint32_t InternalConstBufferRegCount = 2;

    // Magic number describing an invalid or unmapped user data entry
    static constexpr uint8 InvalidReg = UINT8_MAX;

#if VKI_RAY_TRACING
    static constexpr uint32_t MaxTraceRayResourceNodeCount = 16;
    static constexpr uint32_t MaxTraceRayUserDataNodeCount = 1;
    static constexpr uint32_t MaxTraceRayUserDataRegCount  = 1;
#endif

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
#if VKI_RAY_TRACING
        // Denotes if GpuRT resource mappings will need to be added to this pipeline layout
        bool                hasRayTracing;
#endif
    };

    typedef VkPipelineLayout ApiType;

    VkResult BuildLlpcPipelineMapping(
        const uint32_t              stageMask,
        const VbBindingInfo*        pVbInfo,
        const bool                  appendFetchShaderCb,
#if VKI_RAY_TRACING
        const bool                  appendRtCaptureReplayCb,
#endif
        void*                       pBuffer,
        Vkgc::ResourceMappingData*  pResourceMapping,
        Vkgc::ResourceLayoutScheme* pLayoutScheme) const;

    static VkResult Create(
        const Device*                       pDevice,
        const VkPipelineLayoutCreateInfo*   pCreateInfo,
        const VkAllocationCallbacks*        pAllocator,
        VkPipelineLayout*                   pPipelineLayout);

    VkResult Destroy(
        Device*                             pDevice,
        const VkAllocationCallbacks*        pAllocator);

    uint64_t GetApiHash() const
        { return m_apiHash; }

    const PipelineInfo* GetPipelineInfo() const
        { return &m_pipelineInfo; }

    const Info& GetInfo() const
        { return m_info; }

    PipelineLayoutScheme GetScheme() const
        { return m_info.userDataLayout.scheme; }

    // Descriptor set layouts in this pipeline layout
    const SetUserDataLayout& GetSetUserData(const uint32_t setIndex) const
    {
        return static_cast<const SetUserDataLayout*>(Util::VoidPtrInc(this, sizeof(*this)))[setIndex];
    }

    // Original descriptor set layout pointers
    const DescriptorSetLayout* GetSetLayouts(const uint32_t setIndex) const
    {
        return static_cast<const DescriptorSetLayout* const*>(
            Util::VoidPtrInc(this, sizeof(*this) + SetUserDataLayoutSize()))[setIndex];
    }

    DescriptorSetLayout* GetSetLayouts(const uint32_t setIndex)
    {
        return static_cast<DescriptorSetLayout**>(
            Util::VoidPtrInc(this, sizeof(*this) + SetUserDataLayoutSize()))[setIndex];
    }

#if VKI_RAY_TRACING
    uint32_t GetDispatchRaysUserData() const;
#endif

protected:
    static VkResult ConvertCreateInfo(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn,
        Info*                             pInfo,
        PipelineInfo*                     pPipelineInfo,
        SetUserDataLayout*                pSetUserDataLayouts);

    static void ProcessPushConstantsInfo(
        const VkPipelineLayoutCreateInfo* pIn,
        uint32_t*                         pPushConstantsSizeInBytes,
        uint32_t*                         pPushConstantsUserDataNodeCount
    );

    static VkResult BuildCompactSchemeInfo(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn,
        const uint32_t                    pushConstantsSizeInBytes,
        const uint32_t                    pushConstantsUserDataNodeCount,
        Info*                             pInfo,
        PipelineInfo*                     pPipelineInfo,
        SetUserDataLayout*                pSetUserDataLayouts);

    static VkResult BuildIndirectSchemeInfo(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn,
        const uint32_t                    pushConstantsSizeInBytes,
        Info*                             pInfo,
        PipelineInfo*                     pPipelineInfo,
        SetUserDataLayout*                pSetUserDataLayouts);

    static PipelineLayoutScheme DeterminePipelineLayoutScheme(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pIn);

#if VKI_RAY_TRACING
    static bool HasRayTracing(
        const VkPipelineLayoutCreateInfo* pIn);
#endif

    PipelineLayout(
        const Device*       pDevice,
        const Info&         info,
        const PipelineInfo& pipelineInfo,
        uint64_t            apiHash);

    ~PipelineLayout() { }

    VkResult BuildCompactSchemeLlpcPipelineMapping(
        const uint32_t             stageMask,
        const VbBindingInfo*       pVbInfo,
        const bool                 appendFetchShaderCb,
#if VKI_RAY_TRACING
        const bool                 appendRtCaptureReplayCb,
#endif
        void*                      pBuffer,
        Vkgc::ResourceMappingData* pResourceMapping) const;

    void BuildIndirectSchemeLlpcPipelineMapping(
        const uint32_t             stageMask,
        const VbBindingInfo*       pVbInfo,
        const bool                 appendFetchShaderCb,
#if VKI_RAY_TRACING
        const bool                 appendRtCaptureReplayCb,
#endif
        void*                      pBuffer,
        Vkgc::ResourceMappingData* pResourceMapping) const;

    void BuildLlpcStaticMapping(
        const DescriptorSetLayout*              pLayout,
        const uint32_t                          visibility,
        const uint32_t                          setIndex,
        const DescriptorSetLayout::BindingInfo& binding,
        Vkgc::ResourceMappingNode*              pNode,
        Vkgc::StaticDescriptorValue*            pDescriptorRangeValue,
        uint32_t*                               pDescriptorRangeCount) const;

    void BuildLlpcDynamicMapping(
        const uint32_t                          setIndex,
        const uint32_t                          userDataRegBase,
        const DescriptorSetLayout::BindingInfo& binding,
        Vkgc::ResourceMappingNode*              pNode) const;

    void BuildLlpcVertexBufferTableMapping(
        const VbBindingInfo*           pVbInfo,
        const uint32_t                 offsetInDwords,
        const uint32_t                 sizeInDwords,
        Vkgc::ResourceMappingRootNode* pNode,
        uint32_t*                      pNodeCount) const;

    void BuildLlpcTransformFeedbackMapping(
        const uint32_t                 stageMask,
        const uint32_t                 offsetInDwords,
        const uint32_t                 sizeInDwords,
        Vkgc::ResourceMappingRootNode* pNode,
        uint32_t*                      pNodeCount) const;

    void BuildLlpcInternalConstantBufferMapping(
        const uint32_t                 stageMask,
        const uint32_t                 offsetInDwords,
        const uint32_t                 binding,
        Vkgc::ResourceMappingRootNode* pNode,
        uint32_t*                      pNodeCount) const;

#if VKI_RAY_TRACING
    void BuildLlpcRayTracingDispatchArgumentsMapping(
        const uint32_t                 stageMask,
        const uint32_t                 offsetInDwords,
        const uint32_t                 sizeInDwords,
        Vkgc::ResourceMappingRootNode* pRootNode,
        uint32_t*                      pRootNodeCount,
        Vkgc::ResourceMappingNode*     pStaNode,
        uint32_t*                      pStaNodeCount) const;
#endif

    void BuildLlpcDebugPrintfMapping(
        const uint32_t                 stageMask,
        const uint32_t                 offsetInDwords,
        const uint32_t                 sizeInDwords,
        Vkgc::ResourceMappingRootNode* pRootNode,
        uint32_t*                      pRootNodeCount,
        Vkgc::ResourceMappingNode*     pStaNode,
        uint32_t*                      pStaNodeCount) const;

    static void ReserveAlternatingThreadGroupUserData(
        const Device*                     pDevice,
        const VkPipelineLayoutCreateInfo* pPipelineLayoutInfo,
        uint32_t*                         pUserDataNodeCount,
        uint32_t*                         pUSerDataRegCount,
        uint32_t*                         pThreadGroupReversalRegBase);

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
