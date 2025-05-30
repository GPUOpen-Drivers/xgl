/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_pipeline_layout.h"
#include "include/internal_mem_mgr.h"

#include "palFile.h"
#include "palPipelineAbi.h"
#include "debug_printf.h"

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
struct ShaderStageInfo;
struct ShaderModuleHandle;

// Structure containing information about a retrievable pipeline binary.
struct PipelineBinaryInfo
{
    Vkgc::BinaryData      pipelineBinary;
    Util::MetroHash::Hash binaryHash;
};

// Structure containing information about pipeline resource node mapping buffer, like buffer size,
// count of root node and count of resource node.
struct MappingBufferLayout
{
    // The amount of buffer space needed in the mapping buffer.
    size_t              mappingBufferSize;
    // Max. number of ResourceMappingNodes needed by all layouts in the chain, including the extra nodes
    // required by the extra set pointers, and any resource nodes required by potential internal tables.
    uint32_t            numRsrcMapNodes;
    // Number of resource mapping nodes used for the user data nodes
    uint32_t            numUserDataNodes;
};

// Structure containing resouce information about a pipeline binary.
struct PipelineResourceLayout
{
    // TODO: Legacy pipeline layout could be removed
    const PipelineLayout*     pPipelineLayout;

    // Top-level user data layout information of pipeline
    UserDataLayout            userDataLayout;

    // Total number of user data registers used in this pipeline layout
    uint32_t                  userDataRegCount;

    MappingBufferLayout       mappingBufferLayout;

#if VKI_RAY_TRACING
    bool                      hasRayTracing;
#endif
};

constexpr uint32 MaxPipelineBinaryInfoCount = Util::Max(MaxPalDevices, static_cast<uint32>(GraphicsLibraryCount));

// If a pipeline is created with VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR set, it must retain its binaries so that we
// can create VkPipelineBinaryKHR objects from it at any time. We can't rely on our in-memory cache, because it can be
// disabled or have its entries evicted.  This struct lets the pipeline store up to MaxPalDevices binaries and retrieve
// them by key or device index.
struct PipelineBinaryStorage
{
    // For monolithic pipelines this stores a single packed blob per device (same as how caching works).  For graphics
    // pipeline libraries, this stores an elf binary blob per graphics library type.
    PipelineBinaryInfo binaryInfo[MaxPipelineBinaryInfoCount];
    uint32             binaryCount;
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
    SampleLocations,
    FragmentShadingRateStateKhr,
    LineStipple,
    ViewportCount,
    ScissorCount,
    CullMode,
    FrontFace,
    PrimitiveTopology,
    VertexInputBindingStride,
    DepthTestEnable,
    DepthWriteEnable,
    DepthCompareOp,
    DepthBoundsTestEnable,
    StencilTestEnable,
    StencilOp,
    ColorWriteEnable,
    RasterizerDiscardEnable,
    PrimitiveRestartEnable,
    DepthBiasEnable,
    VertexInput,
    TessellationDomainOrigin,
    DepthClampEnable,
    PolygonMode,
    RasterizationSamples,
    SampleMask,
    AlphaToCoverageEnable,
    AlphaToOneEnable,
    LogicOp,
    LogicOpEnable,
    ColorBlendEnable,
    ColorBlendEquation,
    ColorWriteMask,
    RasterizationStream,
    ConservativeRasterizationMode,
    ExtraPrimitiveOverestimationSize,
    DepthClipEnable,
    SampleLocationsEnable,
    ProvokingVertexMode,
    LineRasterizationMode,
    LineStippleEnable,
    DepthClipNegativeOneToOne,
    DepthClampControl,

    DynamicStatesInternalCount
};

// =====================================================================================================================
// Common extension structures for pipeline creation
struct PipelineExtStructs
{
    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeedbackCreateInfoEXT;
    const VkPipelineBinaryInfoKHR* pPipelineBinaryInfoKHR;
    const VkPipelineRobustnessCreateInfoEXT* pPipelineRobustnessCreateInfoEXT;
};

// =====================================================================================================================
// Common extension structures for pipeline shader stage creation
struct PipelineShaderStageExtStructs
{
    const VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT* pPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT;
    const VkShaderModuleCreateInfo* pShaderModuleCreateInfo;
    const VkPipelineShaderStageModuleIdentifierCreateInfoEXT* pPipelineShaderStageModuleIdentifierCreateInfoEXT;
    const VkPipelineRobustnessCreateInfoEXT* pPipelineRobustnessCreateInfoEXT;
};

// =====================================================================================================================
// Base class of all pipeline objects.
class Pipeline
{
public:
    virtual ~Pipeline() = default;

    virtual VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator);

    const UserDataLayout* GetUserDataLayout() const { return &m_userDataLayout; }

    static VK_FORCEINLINE Pipeline* BaseObjectFromHandle(VkPipeline pipeline)
        { return reinterpret_cast<Pipeline*>(pipeline); }

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

    uint64_t PalPipelineHash() const { return m_palPipelineHash; }

    uint64_t GetApiHash() const { return m_apiHash; }

    bool GetBinary(
        Pal::ShaderType     shaderType,
        PipelineBinaryInfo* pBinaryInfo) const;

    static void BuildPipelineResourceLayout(
        const Device*                     pDevice,
        const PipelineLayout*             pPipelineLayout,
        VkPipelineBindPoint               pipelineBindPoint,
        VkPipelineCreateFlags2KHR         flags,
        PipelineResourceLayout*           pResourceLayout);

    VkPipelineBindPoint GetType() const { return m_type; }

    // This function returns true if any of the bits in the given state mask (corresponding to shifted values of
    // VK_DYNAMIC_STATE_*) should be programmed by the pipeline when it is bound (instead of by the application via
    // vkCmdSet*).
    bool ContainsStaticState(DynamicStatesInternal dynamicState) const
        { return ((m_staticStateMask & (1ULL << static_cast<uint32_t>(dynamicState))) != 0); }

    bool ContainsDynamicState(DynamicStatesInternal dynamicState) const
        { return ((m_staticStateMask & (1ULL << static_cast<uint32_t>(dynamicState))) == 0); }

    uint32_t GetAvailableAmdIlSymbol(uint32_t shaderStageMask) const;

    VkResult GetShaderDisassembly(
        const Device*                 pDevice,
        const Pal::IPipeline*         pPalPipeline,
        Util::Abi::PipelineSymbolType pipelineSymbolType,
        Pal::ShaderType               shaderType,
        size_t*                       pBufferSize,
        void*                         pBuffer) const;

#if VKI_RAY_TRACING
    uint32_t GetDispatchRaysUserDataOffset() const { return m_dispatchRaysUserDataOffset; }

    bool HasRayTracing() const { return m_hasRayTracing; }

    static uint32_t GetDispatchRaysUserData(
        const PipelineResourceLayout* pResourceLayout);
#endif

    void ClearFormatString()
    {
        if (m_pFormatStrings != nullptr)
        {
            m_pFormatStrings->Reset();
        }
    }

    const PrintfFormatMap* GetFormatStrings() const
    {
        VK_ASSERT(m_pFormatStrings != nullptr);
        return m_pFormatStrings;
    }

    PrintfFormatMap* GetFormatStrings()
    {
        if (m_pFormatStrings == nullptr)
        {
            void* pBuffer =
                m_pDevice->VkInstance()->AllocMem(sizeof(PrintfFormatMap), VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
            if (pBuffer != nullptr)
            {
                m_pFormatStrings = VK_PLACEMENT_NEW(pBuffer) PrintfFormatMap(32, m_pDevice->VkInstance()->Allocator());
                m_pFormatStrings->Init();
            }
        }
        return m_pFormatStrings;
    }

    static void GenerateHashFromSpecializationInfo(
        const VkSpecializationInfo& desc,
        Util::MetroHash128*         pHasher);

    static void GenerateHashFromShaderStageCreateInfo(
        const ShaderStageInfo& stageInfo,
        Util::MetroHash128*    pHasher);

    static void ElfHashToCacheId(
        const Device*                pDevice,
        uint32_t                     deviceIdx,
        const Util::MetroHash::Hash& elfHash,
        const PipelineOptimizerKey&  pipelineOptimizerKey,
        Util::MetroHash::Hash*       pCacheId
    );

    const PipelineBinaryStorage* GetBinaryStorage() const
        { return m_pBinaryStorage; }

    // See the implementation note about memory ownership behavior.
    static void InsertBinaryData(
        PipelineBinaryStorage*          pBinaryStorage,
        const uint32                    binaryIndex,
        const Util::MetroHash::Hash&    key,
        const size_t                    dataSize,
        const void*                     pData);

    VkResult FreeBinaryStorage(
        const VkAllocationCallbacks*    pAllocator);

    static void FreeBinaryStorage(
        PipelineBinaryStorage*          pBinaryStorage,
        const VkAllocationCallbacks*    pAllocator);

    static void FreeTempModules(
        const Device*       pDevice,
        const uint32_t      maxStageCount,
        ShaderModuleHandle* pTempModules);

protected:
    Pipeline(
        Device* const               pDevice,
#if VKI_RAY_TRACING
        bool                        hasRayTracing,
#endif
        VkPipelineBindPoint         type);

    void Init(
        Pal::IPipeline**            pPalPipeline,
        const UserDataLayout*       pLayout,
        PipelineBinaryStorage*      pBinaryStorage,
        uint64_t                    staticStateMask,
#if VKI_RAY_TRACING
        uint32_t                     dispatchRaysUserDataOffset,
#endif
        const Util::MetroHash::Hash& cacheHash,
        uint64_t                     apiHash);

    static VkPipelineCreateFlags2KHR GetCacheIdControlFlags(
        VkPipelineCreateFlags2KHR in);

    static void GenerateHashFromShaderStageCreateInfo(
        const VkPipelineShaderStageCreateInfo& desc,
        Util::MetroHash128*                    pHasher);

    static void GenerateHashFromDynamicStateCreateInfo(
        const VkPipelineDynamicStateCreateInfo& desc,
        Util::MetroHash128*                     pHasher);

    static VkResult BuildShaderStageInfo(
        const Device*                          pDevice,
        const uint32_t                         stageCount,
        const VkPipelineShaderStageCreateInfo* pStages,
        uint32_t                               (*pfnGetOutputIdx)(const uint32_t inputIdx,
                                                                  const uint32_t stageIdx),
        ShaderStageInfo*                       pShaderStageInfo,
        ShaderModuleHandle*                    pTempModules,
        PipelineCreationFeedback*              pFeedbacks);

    // Extract extension structs that are common between pipeline types from their respective Vk*PipelineCreateInfo
    static void HandleExtensionStructs(
        const void*                         pNext,
        PipelineExtStructs*                 pExtStructs);

    // Extract extension structs that are common between pipeline types from their respective Vk*PipelineCreateInfo
    static void HandleShaderStageExtensionStructs(
        const void*                            pNext,
        PipelineShaderStageExtStructs*         pExtStructs);

    // Initialize the VkPipelineRobustnessCreateInfoEXT struct based on the pipeline's extStruct
    static bool InitPipelineRobustness(
        const VkPipelineRobustnessCreateInfoEXT* incomingRobustness,
        VkPipelineRobustnessCreateInfoEXT*       pCurrentRobustness);

    // Update the VkPipelineRobustnessCreateInfoEXT struct based on the pipeline's shader stage or pipeline library
    static void UpdatePipelineRobustness(
        const VkPipelineRobustnessCreateInfoEXT* incomingRobustness,
        VkPipelineRobustnessCreateInfoEXT*       pCurrentRobustness);

    // Update the VkPipelineRobustnessBufferBehaviorEXT struct based on the pipeline's shader stage or pipeline library
    static void UpdatePipelineRobustnessBufferBehavior(
        const VkPipelineRobustnessBufferBehaviorEXT incomingRobustness,
        VkPipelineRobustnessBufferBehaviorEXT*      pCurrentRobustness);

    // Update the VkPipelineRobustnessImageBehaviorEXT struct based on the pipeline's shader stage or pipeline library
    static void UpdatePipelineRobustnessImageBehavior(
        const VkPipelineRobustnessImageBehaviorEXT incomingRobustness,
        VkPipelineRobustnessImageBehaviorEXT*      pCurrentRobustness);

    Device* const                      m_pDevice;
    UserDataLayout                     m_userDataLayout;
    Pal::IPipeline*                    m_pPalPipeline[MaxPalDevices];
    uint64_t                           m_palPipelineHash; // Unique hash for Pal::Pipeline
    uint64_t                           m_staticStateMask; // Bitfield to detect which subset of pipeline state is
                                                          // static (written at bind-time as opposed to via vkCmd*).
    uint64_t                           m_apiHash;
    VkPipelineBindPoint                m_type;
    Util::MetroHash::Hash              m_cacheHash;       // Cache Id of pipeline binary on default PAL device

#if VKI_RAY_TRACING
    const bool                         m_hasRayTracing;
    uint32_t                           m_dispatchRaysUserDataOffset;
#endif

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);

    PipelineBinaryStorage*              m_pBinaryStorage;
    PrintfFormatMap*                    m_pFormatStrings;
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

VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetPipelineIndirectDeviceAddressNV(
    VkDevice                                        device,
    const VkPipelineIndirectDeviceAddressInfoNV*    pInfo);

VKAPI_ATTR void VKAPI_CALL vkGetPipelineIndirectMemoryRequirementsNV(
    VkDevice                                        device,
    const VkComputePipelineCreateInfo*              pCreateInfo,
    VkMemoryRequirements2*                          pMemoryRequirements);
};

} // namespace vk

#endif /* __VK_PIPELINE_H__ */
