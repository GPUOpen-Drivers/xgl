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

#ifndef __VK_COMPUTE_PIPELINE_H__
#define __VK_COMPUTE_PIPELINE_H__

#pragma once

#include "include/vk_pipeline.h"
#include "include/internal_mem_mgr.h"

#include "palPipeline.h"

namespace vk
{

class CmdBuffer;
class Device;
class PipelineCache;

// =====================================================================================================================
// Vulkan implementation of compute pipelines created by vkCreateComputePipeline
class ComputePipeline : public Pipeline, public NonDispatchable<VkPipeline, ComputePipeline>
{
public:
    static VkResult Create(
        Device*                                pDevice,
        PipelineCache*                         pPipelineCache,
        const VkComputePipelineCreateInfo*     pCreateInfo,
        const VkAllocationCallbacks*           pAllocator,
        VkPipeline*                            pPipeline);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    void BindToCmdBuffer(
        CmdBuffer*                           pCmdBuffer) const;

    void BindToCmdBuffer(
        CmdBuffer*                           pCmdBuffer,
        const Pal::DynamicComputeShaderInfo& computeShaderInfo) const;

    static void BindNullPipeline(CmdBuffer* pCmdBuffer);

    // This function returns true if any of the bits in the given state mask (corresponding to shifted values of
    // VK_DYNAMIC_STATE_*) should be programmed by the pipeline when it is bound (instead of by the application via
    // vkCmdSet*).
    VK_INLINE bool PipelineSetsState(DynamicStatesInternal dynamicState) const
    {
        return ((m_info.staticStateMask & (1UL << static_cast<uint32_t>(dynamicState))) != 0);
    }

protected:
    // Immediate state info that will be written during Bind() but is not
    // encapsulated within a state object.
    //
    // NOTE: This structure needs to be revisited when the new PAL state headers
    // are in place.
    struct ImmedInfo
    {
        // Bitfield to detect which subset of pipeline state is static (written at bind-time).
        uint32_t                      staticStateMask;

        Pal::DynamicComputeShaderInfo computeWaveLimitParams;

        // Static pipeline parameter token values.  These can be used to efficiently redundancy check static pipeline
        // state programming during pipeline binds.
        struct
        {
            uint32_t waveLimits;
        } staticTokens;
    };

    ComputePipeline(
        Device* const                        pDevice,
        Pal::IPipeline**                     pPalPipeline,
        const PipelineLayout*                pPipelineLayout,
        PipelineBinaryInfo*                  pPipelineBinary,
        const ImmedInfo&                     immedInfo);

    void CreateStaticState();
    void DestroyStaticState(const VkAllocationCallbacks* pAllocator);

    // Converted creation info parameters of the Vulkan compute pipeline
    struct CreateInfo
    {
        ImmedInfo                              immedInfo;
        Pal::ComputePipelineCreateInfo         pipeline;
        const PipelineLayout*                  pLayout;
    };

    static void ConvertComputePipelineInfo(
        Device*                            pDevice,
        const VkComputePipelineCreateInfo* pIn,
        CreateInfo*                        pOutInfo);

    static VkResult CreateComputePipelineBinaries(
        Device*                            pDevice,
        PipelineCache*                     pPipelineCache,
        CreateInfo*                        pCreateInfo,
        size_t                             pipelineBinarySizes[MaxPalDevices],
        void*                              pPipelineBinaries[MaxPalDevices]);

private:
    ImmedInfo m_info; // Immediate state that will go in CmdSet* functions
};

} // namespace vk

#endif /* __VK_COMPUTE_PIPELINE_H__ */
