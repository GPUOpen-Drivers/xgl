/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_RAY_TRACING_PIPELINE_H__
#define __VK_RAY_TRACING_PIPELINE_H__

#pragma once

#include "include/vk_pipeline.h"
#include "include/internal_mem_mgr.h"

#include "palPipeline.h"
#include "palVector.h"

namespace Vkgc
{
struct RayTracingShaderIdentifier;
struct RayTracingShaderProperty;
}

namespace Util
{
namespace MetroHash
{
struct Hash;
}
}

namespace vk
{

class CmdBuffer;
class DeferredHostOperation;
class Device;
class PipelineCache;

static constexpr uint64_t RayTracingInvalidShaderId = 0;

typedef Util::Vector<VkPipelineShaderStageCreateInfo, 16, PalAllocator>       ShaderStageList;
typedef Util::Vector<VkRayTracingShaderGroupCreateInfoKHR, 16, PalAllocator>  ShaderGroupList;

struct ShaderGroupStackSizes
{
    VkDeviceSize generalSize;
    VkDeviceSize closestHitSize;
    VkDeviceSize anyHitSize;
    VkDeviceSize intersectionSize;
};

struct ShaderGroupInfo
{
    VkRayTracingShaderGroupTypeKHR type;
    VkShaderStageFlags             stages;
};

struct CaptureReplayVaMappingBufferInfo
{
    uint32_t dataSize;
    void*    pData;
};

class PipelineImplCreateInfo
{
public:
    PipelineImplCreateInfo(Device* const   pDevice);

    ~PipelineImplCreateInfo();

    void AddToStageList(
        const VkPipelineShaderStageCreateInfo& stageInfo);

    void AddToGroupList(
        const VkRayTracingShaderGroupCreateInfoKHR& groupInfo);

    uint32_t GetStageCount() const
        { return m_stageCount; }

    void SetStageCount(uint32_t cnt)
        { m_stageCount = cnt; }

    const ShaderStageList& GetStageList() const
        { return m_stageList; }

    uint32_t GetGroupCount() const
        { return m_groupCount; }

    void SetGroupCount(uint32_t cnt)
        { m_groupCount = cnt; }

    const ShaderGroupList& GetGroupList() const
        { return m_groupList; }

    uint32_t GetMaxRecursionDepth() const
        { return m_maxRecursionDepth; }

    void SetMaxRecursionDepth(uint32_t val)
        { m_maxRecursionDepth = val; }

private:
    uint32_t            m_stageCount;
    ShaderStageList     m_stageList;
    uint32_t            m_groupCount;
    ShaderGroupList     m_groupList;
    uint32_t            m_maxRecursionDepth;
};

// =====================================================================================================================
// Vulkan implementation of ray tracing pipelines created by vkCreateRayTracingPipelinesKHR
class RayTracingPipeline final : public Pipeline, public NonDispatchable<VkPipeline, RayTracingPipeline>
{
public:
    static RayTracingPipeline* ObjectFromHandle(VkPipeline pipeline)
        { return NonDispatchable<VkPipeline, RayTracingPipeline>::ObjectFromHandle(pipeline); }

    static VkResult Create(
        Device*                                  pDevice,
        DeferredHostOperation*                   pDeferredOperation,
        PipelineCache*                           pPipelineCache,
        uint32_t                                 count,
        const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
        const VkAllocationCallbacks*             pAllocator,
        VkPipeline*                              pPipelines);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    VkResult CreateImpl(
        PipelineCache*                           pPipelineCache,
        const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
        PipelineCreateFlags                      flags,
        const VkAllocationCallbacks*             pAllocator,
        DeferredWorkload*                        pDeferredWorkload);

    void BindToCmdBuffer(
        CmdBuffer*                           pCmdBuffer,
        const Pal::DynamicComputeShaderInfo& dynamicBindInfo) const;

    const Pal::DynamicComputeShaderInfo& GetBindInfo() const { return m_info.computeShaderInfo; }

    void GetRayTracingShaderGroupHandles(
        uint32_t                             deviceIndex,
        uint32_t                             firstGroup,
        uint32_t                             groupCount,
        size_t                               dataSize,
        void*                                pData);

    VkDeviceSize GetRayTracingShaderGroupStackSize(
        uint32_t                            deviceIndex,
        uint32_t                            group,
        VkShaderGroupShaderKHR              groupShader) const;

    static void BindNullPipeline(CmdBuffer* pCmdBuffer);

    uint32_t GetAttributeSize() const
        { return m_attributeSize; }

    uint32_t GetDefaultPipelineStackSize(uint32_t deviceIdx) const
        { return m_defaultPipelineStackSizes[deviceIdx]; }

    uint32_t GetShaderGroupCount() const
        { return m_shaderGroupCount; }

    const Vkgc::RayTracingShaderIdentifier* GetShaderGroupHandles(uint32_t deviceIdx) const
        { return m_pShaderGroupHandles[deviceIdx]; }

    const ShaderGroupInfo* GetShaderGroupInfos() const
        { return m_pShaderGroupInfos; }

    Pal::gpusize GetTraceRayGpuVa(uint32_t deviceIdx) const
        { return m_traceRayGpuVas[deviceIdx]; }

    const PipelineImplCreateInfo& GetCreateInfo() const
        { return m_createInfo; }

    bool IsInlinedShaderEnabled() const
        { return m_shaderLibraryCount == 0; }

    uint32_t GetShaderLibraryCount() const
        { return m_shaderLibraryCount; }

    const Pal::IShaderLibrary* PalShaderLibrary(uint32_t idx) const
        { return m_ppShaderLibraries[idx]; }

    const Pal::IShaderLibrary*const* GetShaderLibraries() const
        { return m_ppShaderLibraries; }

    bool CheckHasTraceRay() const
        { return m_hasTraceRay; }

    void UpdatePipelineImplCreateInfo(const VkRayTracingPipelineCreateInfoKHR* pCreateInfoIn);

    static void ConvertStaticPipelineFlags(const Device* pDevice,
                                           uint32_t* pStaticFlags,
                                           uint32_t* pTriangleCompressMode,
                                           uint32_t* pCounterMode,
                                           uint32_t  pipelineFlags
);

    void GetDispatchSize(uint32_t* pDispatchSizeX,
                         uint32_t* pDispatchSizeY,
                         uint32_t* pDispatchSizeZ,
                         uint32_t  width,
                         uint32_t  height,
                         uint32_t  depth) const;

protected:
    // Make sure that this value should be equal to Bil::RayTracingTileWidth defined in bilInstructionRayTracing.h
    // and the value used to calculate dispatch dim in InitExecuteIndirect.hlsl
    static constexpr uint32_t RayTracingTileWidth = 8;

    // Immediate state info that will be written during Bind() but is not
    // encapsulated within a state object.
    //
    // NOTE: This structure needs to be revisited when the new PAL state headers
    // are in place.
    struct ImmedInfo
    {
        Pal::DynamicComputeShaderInfo computeShaderInfo;
    };

    RayTracingPipeline(
        Device* const   pDevice);

    void Init(
        Pal::IPipeline**                     ppPalPipeline,
        uint32_t                             shaderLibraryCount,
        Pal::IShaderLibrary**                ppPalShaderLibrary,
        const PipelineLayout*                pPipelineLayout,
        const ShaderOptimizerKey*            pShaderOptKeys,
        const ImmedInfo&                     immedInfo,
        uint64_t                             staticStateMask,
        uint32_t                             nativeShaderCount,
        uint32_t                             totalShaderCount,
        uint32_t                             shaderGroupCount,
        Vkgc::RayTracingShaderIdentifier*    pShaderGroupHandles[MaxPalDevices],
        ShaderGroupStackSizes*               pShaderGroupStackSizes[MaxPalDevices],
        ShaderGroupInfo*                     pShaderGroupInfos,
        uint32_t                             attributeSize,
        Pal::gpusize                         traceRayGpuVas[MaxPalDevices],
        uint32_t                             dispatchRaysUserDataOffset,
        const Util::MetroHash::Hash&         cacheHash,
        uint64_t                             apiHash,
        const Util::MetroHash::Hash&         elfHash);

    uint32_t UpdateShaderGroupIndex(uint32_t shader, uint32_t idx);

    VkResult ProcessCaptureReplayHandles(Vkgc::RayTracingShaderIdentifier*     pShaderGroupHandles,
                                         const VkPipelineLibraryCreateInfoKHR* pLibraryInfo,
                                         const VkAllocationCallbacks*          pAllocator);

    const CaptureReplayVaMappingBufferInfo& GetCaptureReplayVaMappingBufferInfo() const
        { return m_captureReplayVaMappingBufferInfo; }

    const Util::MetroHash::Hash& GetElfHash() const
        { return m_elfHash; }

    const ShaderOptimizerKey* GetShaderOptKeys() const
        { return m_pShaderOptKeys; }

    uint32_t GetNativeShaderCount() const
        { return m_nativeShaderCount; }

    uint32_t GetTotalShaderCount() const
        { return m_totalShaderCount; }

    // Converted creation info parameters of the Vulkan ray tracing pipeline
    struct CreateInfo
    {
        ImmedInfo                              immedInfo;
        uint64_t                               staticStateMask;
        Pal::ComputePipelineCreateInfo         pipeline;
        const PipelineLayout*                  pLayout;

        void*                                  pTempBuffer;

        bool                                   deferralRequested;
    };

    static void ConvertRayTracingPipelineInfo(
        Device*                                  pDevice,
        const VkRayTracingPipelineCreateInfoKHR* pIn,
        CreateInfo*                              pOutInfo);

    static VkResult CreateRayTracingPipelineBinaries(
        Device*                            pDevice,
        PipelineCache*                     pPipelineCache,
        CreateInfo*                        pCreateInfo,
        size_t                             pipelineBinarySizes[MaxPalDevices],
        void*                              pPipelineBinaries[MaxPalDevices]);

    static void BuildApiHash(
        const VkRayTracingPipelineCreateInfoKHR* pCreateInfo,
        PipelineCreateFlags                      flags,
        Util::MetroHash::Hash*                   pElfHash,
        uint64_t*                                pApiHash);

    // Return true if the shader id was found and mapped to a shader handle with gpu address.
    static bool MapShaderIdToShaderHandle(
        Pal::ShaderLibraryFunctionInfo*   pIndirectFuncList,
        uint32_t*                         pShaderNameMap,
        uint32_t                          shaderPropsCount,
        Vkgc::RayTracingShaderProperty*   pShaderProp,
        uint64_t*                         pShaderId);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(RayTracingPipeline);

    ImmedInfo                         m_info; // Immediate state that will go in CmdSet* functions
    const ShaderOptimizerKey*         m_pShaderOptKeys;

    // Shader Groups
    uint32_t                          m_shaderGroupCount;
    Vkgc::RayTracingShaderIdentifier* m_pShaderGroupHandles[MaxPalDevices];
    ShaderGroupStackSizes*            m_pShaderGroupStackSizes[MaxPalDevices];
    ShaderGroupInfo*                  m_pShaderGroupInfos;

    uint32_t                          m_nativeShaderCount;  // number of non-library shaders
    uint32_t                          m_totalShaderCount;   // all shaders, including libraries

    uint32_t                          m_attributeSize;
    Pal::gpusize                      m_traceRayGpuVas[MaxPalDevices];
    uint32_t                          m_shaderLibraryCount;
    Pal::IShaderLibrary**             m_ppShaderLibraries;
    uint32_t                          m_defaultPipelineStackSizes[MaxPalDevices];

    PipelineImplCreateInfo            m_createInfo;
    bool                              m_hasTraceRay;
    Util::MetroHash::Hash             m_elfHash;

    CaptureReplayVaMappingBufferInfo  m_captureReplayVaMappingBufferInfo;
};

} // namespace vk

#endif /* __VK_RAY_TRACING_PIPELINE_H__ */
