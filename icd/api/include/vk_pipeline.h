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

#ifndef __VK_PIPELINE_H__
#define __VK_PIPELINE_H__

#pragma once

#include "include/khronos/vulkan.h"
#include "include/vk_utils.h"
#include "include/vk_defines.h"
#include "include/vk_dispatch.h"
#include "include/internal_mem_mgr.h"

#include "palFile.h"
#include "palPipelineAbi.h"

namespace Pal
{

class IDevice;
class IPipeline;

}

namespace Util
{

class MetroHash128;

}

namespace vk
{

class  Device;
class  ComputePipeline;
class  GraphicsPipeline;
class  PipelineLayout;
struct RuntimeSettings;

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

    // Base user data register index to use for transform feedback.
    uint32_t transformFeedbackRegBase;
    // Number of user data registers used for transform feedback
    uint32_t transformFeedbackRegCount;
};

// Structure containing information about a retrievable pipeline binary.  These are only retained by Pipeline objects
// when specific device extensions (VK_AMD_shader_info/VK_KHR_pipeline_executable_properties) that can query them are
// enabled.
struct PipelineBinaryInfo
{
    static PipelineBinaryInfo* Create(size_t size, const void* pBinary, const VkAllocationCallbacks* pAllocator);

    void Destroy(const VkAllocationCallbacks* pAllocator);

    size_t binaryByteSize;
    void*  pBinary;
};

enum class DynamicStatesInternal : uint32_t
{
    Viewport = 0,
    Scissor,
    LineWidth,
    DepthBias,
    BlendConstants,
    DepthBounds,
    StencilCompareMask,
    StencilWriteMask,
    StencilReference,
    SampleLocationsExt,
    FragmentShadingRateStateKhr,
    LineStippleExt,
    ViewportCount,
    ScissorCount,
    CullModeExt,
    FrontFaceExt,
    PrimitiveTopologyExt,
    VertexInputBindingStrideExt,
    DepthTestEnableExt,
    DepthWriteEnableExt,
    DepthCompareOpExt,
    DepthBoundsTestEnableExt,
    StencilTestEnableExt,
    StencilOpExt,
    ColorWriteEnableExt,
    DynamicStatesInternalCount
};

// =====================================================================================================================
// Base class of all pipeline objects.
class Pipeline
{
public:
    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    const UserDataLayout* GetUserDataLayout(void) const { return &m_userDataLayout; }

    static VK_FORCEINLINE Pipeline* BaseObjectFromHandle(VkPipeline pipeline)
    {
        return reinterpret_cast<Pipeline*>(pipeline);
    }

    const Pal::IPipeline* PalPipeline(int32_t idx) const
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalPipeline[idx];
    }

    Pal::IPipeline* PalPipeline(int32_t idx)
    {
        VK_ASSERT((idx >= 0) && (idx < static_cast<int32_t>(MaxPalDevices)));
        return m_pPalPipeline[idx];
    }

    VK_INLINE uint64_t PalPipelineHash() const
    {
        return m_palPipelineHash;
    }

    VK_INLINE uint64_t GetApiHash() const
        { return m_apiHash; }

    VK_INLINE const PipelineBinaryInfo* GetBinary() const
        { return m_pBinary; }

    VK_INLINE VkPipelineBindPoint GetType() const
        { return m_type; }

    // This function returns true if any of the bits in the given state mask (corresponding to shifted values of
    // VK_DYNAMIC_STATE_*) should be programmed by the pipeline when it is bound (instead of by the application via
    // vkCmdSet*).
    VK_INLINE bool ContainsStaticState(DynamicStatesInternal dynamicState) const
        { return ((m_staticStateMask & (1UL << static_cast<uint32_t>(dynamicState))) != 0); }

    VK_INLINE bool ContainsDynamicState(DynamicStatesInternal dynamicState) const
        { return ((m_staticStateMask & (1UL << static_cast<uint32_t>(dynamicState))) == 0); }

    VkResult GetShaderDisassembly(
        const Device*                 pDevice,
        const Pal::IPipeline*         pPalPipeline,
        Util::Abi::PipelineSymbolType pipelineSymbolType,
        Pal::ShaderType               shaderType,
        size_t*                       pBufferSize,
        void*                         pBuffer) const;

protected:
    Pipeline(
        Device* const         pDevice,
        VkPipelineBindPoint   type);

    virtual ~Pipeline();

    void Init(
        Pal::IPipeline**      pPalPipeline,
        const PipelineLayout* pLayout,
        PipelineBinaryInfo*   pBinary,
        uint32_t              staticStateMask,
        uint64_t              apiHash);

    static void GenerateHashFromSpecializationInfo(
        Util::MetroHash128*         pHasher,
        const VkSpecializationInfo& desc);

    static void GenerateHashFromShaderStageCreateInfo(
        Util::MetroHash128*                    pHasher,
        const VkPipelineShaderStageCreateInfo& desc);

    static void GenerateHashFromDynamicStateCreateInfo(
        Util::MetroHash128*                     pHasher,
        const VkPipelineDynamicStateCreateInfo& desc);

    Device* const                      m_pDevice;
    UserDataLayout                     m_userDataLayout;
    Pal::IPipeline*                    m_pPalPipeline[MaxPalDevices];
    uint64_t                           m_palPipelineHash; // Unique hash for Pal::Pipeline
    uint32_t                           m_staticStateMask; // Bitfield to detect which subset of pipeline state is
                                                          // static (written at bind-time as opposed to via vkCmd*).
    uint64_t                           m_apiHash;
    VkPipelineBindPoint                m_type;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);

    PipelineBinaryInfo*                m_pBinary;
};

namespace entry
{

VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL vkGetShaderInfoAMD(
    VkDevice                                    device,
    VkPipeline                                  pipeline,
    VkShaderStageFlagBits                       shaderStage,
    VkShaderInfoTypeAMD                         infoType,
    size_t*                                     pBufferSize,
    void*                                       pBuffer);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutablePropertiesKHR(
    VkDevice                                    device,
    const VkPipelineInfoKHR*                    pPipelineInfo,
    uint32_t*                                   pExecutableCount,
    VkPipelineExecutablePropertiesKHR*          pProperties);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableStatisticsKHR(
    VkDevice                                    device,
    const VkPipelineExecutableInfoKHR*          pExecutableInfo,
    uint32_t*                                   pStatisticCount,
    VkPipelineExecutableStatisticKHR*           pStatistics);

VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineExecutableInternalRepresentationsKHR(
    VkDevice                                       device,
    const VkPipelineExecutableInfoKHR*             pExecutableInfo,
    uint32_t*                                      pInternalRepresentationCount,
    VkPipelineExecutableInternalRepresentationKHR* pInternalRepresentations);

};

} // namespace vk

#endif /* __VK_PIPELINE_H__ */
