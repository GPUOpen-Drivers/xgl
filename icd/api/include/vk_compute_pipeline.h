/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_COMPUTE_PIPELINE_H__
#define __VK_COMPUTE_PIPELINE_H__

#pragma once

#include "include/vk_pipeline.h"
#include "include/internal_mem_mgr.h"

#include "palPipeline.h"

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
class Device;
class PipelineCache;

// =====================================================================================================================
// Extension structures for pipeline creation
struct ComputePipelineExtStructs : PipelineExtStructs
{
};

// =====================================================================================================================
// Vulkan implementation of compute pipelines created by vkCreateComputePipeline
class ComputePipeline final : public Pipeline, public NonDispatchable<VkPipeline, ComputePipeline>
{
public:
    static VkResult Create(
        Device*                                pDevice,
        PipelineCache*                         pPipelineCache,
        const VkComputePipelineCreateInfo*     pCreateInfo,
        VkPipelineCreateFlags2KHR              flags,
        const VkAllocationCallbacks*           pAllocator,
        VkPipeline*                            pPipeline);

    static VkResult CreateCacheId(
        const Device*                           pDevice,
        const VkComputePipelineCreateInfo*      pCreateInfo,
        VkPipelineCreateFlags2KHR               flags,
        ComputePipelineShaderStageInfo*         pShaderInfo,
        ComputePipelineBinaryCreateInfo*        pBinaryCreateInfo,
        ShaderOptimizerKey*                     pShaderOptimizerKey,
        PipelineOptimizerKey*                   pPipelineOptimizerKey,
        uint64_t*                               pApiPsoHash,
        ShaderModuleHandle*                     pTempModule,
        Util::MetroHash::Hash*                  pCacheIds);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    void BindToCmdBuffer(
        CmdBuffer*                           pCmdBuffer,
        const Pal::DynamicComputeShaderInfo& computeShaderInfo) const;

    static void BindNullPipeline(CmdBuffer* pCmdBuffer);

    const Pal::DynamicComputeShaderInfo& GetBindInfo() const
        { return m_info.computeShaderInfo; }

    const uint32_t* GetOrigThreadgroupDims() const
        { return m_origThreadgroupDims; }

protected:
    // Immediate state info that will be written during Bind() but is not
    // encapsulated within a state object.
    //
    // NOTE: This structure needs to be revisited when the new PAL state headers
    // are in place.
    struct ImmedInfo
    {
        Pal::DynamicComputeShaderInfo computeShaderInfo;
    };

    ComputePipeline(
        Device* const                        pDevice,
        Pal::IPipeline**                     pPalPipeline,
        const PipelineLayout*                pPipelineLayout,
        const ImmedInfo&                     immedInfo,
#if VKI_RAY_TRACING
        bool                                 hasRayTracing,
        uint32_t                             dispatchRaysUserDataOffset,
#endif
        const uint32_t*                      pOrigThreadgroupDims,
        uint64_t                             staticStateMask,
        const Util::MetroHash::Hash&         cacheHash,
        uint64_t                             apiHash);

    // Converted creation info parameters of the Vulkan compute pipeline
    struct CreateInfo
    {
        ImmedInfo                              immedInfo;
        uint64_t                               staticStateMask;
        Pal::ComputePipelineCreateInfo         pipeline;
        const PipelineLayout*                  pLayout;
    };

    static void ConvertComputePipelineInfo(
        Device*                               pDevice,
        const VkComputePipelineCreateInfo*    pIn,
        const ComputePipelineShaderStageInfo& stageInfo,
        CreateInfo*                           pOutInfo);

    static void BuildApiHash(
        const VkComputePipelineCreateInfo*    pCreateInfo,
        VkPipelineCreateFlags2KHR             flags,
        const ComputePipelineShaderStageInfo& stageInfo,
        Util::MetroHash::Hash*                pElfHash,
        uint64_t*                             pApiHash);

    static VkResult CreatePipelineBinaries(
        Device*                                        pDevice,
        const VkComputePipelineCreateInfo*             pCreateInfo,
        const ComputePipelineExtStructs&               extStructs,
        VkPipelineCreateFlags2KHR                      flags,
        const ComputePipelineShaderStageInfo*          pShaderInfo,
        const PipelineOptimizerKey*                    pPipelineOptimizerKey,
        ComputePipelineBinaryCreateInfo*               pBinaryCreateInfo,
        PipelineCache*                                 pPipelineCache,
        Util::MetroHash::Hash*                         pCacheIds,
        Vkgc::BinaryData*                              pPipelineBinaries,
        PipelineMetadata*                              pBinaryMetadata);

    static void FetchPalMetadata(
        PalAllocator* pAllocator,
        const void*   pBinary,
        uint32_t*     pOrigThreadgroupDims);

    // Extracts extension structs from VkComputePipelineCreateInfo
    static void HandleExtensionStructs(
        const VkComputePipelineCreateInfo* pCreateInfo,
        ComputePipelineExtStructs*         pExtStructs);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);

    ImmedInfo m_info; // Immediate state that will go in CmdSet* functions

    uint32_t m_origThreadgroupDims[3];
};

} // namespace vk

#endif /* __VK_COMPUTE_PIPELINE_H__ */
