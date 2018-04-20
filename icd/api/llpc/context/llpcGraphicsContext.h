/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcGraphicsContext.h
 * @brief LLPC header file: contains declaration of class Llpc::GraphicsContext.
 ***********************************************************************************************************************
 */
#pragma once

#include "llpcPipelineContext.h"

namespace Llpc
{

// =====================================================================================================================
// Represents LLPC context for graphics pipeline compilation. Derived from the base class Llpc::Context.
class GraphicsContext: public PipelineContext
{
public:
    GraphicsContext(GfxIpVersion                     gfxIp,
                    const GpuProperty*               pGpuProp,
                    const GraphicsPipelineBuildInfo* pPipelineInfo,
                    MetroHash::Hash*                 pHash);
    virtual ~GraphicsContext();

    virtual ResourceUsage* GetShaderResourceUsage(ShaderStage shaderStage);
    virtual InterfaceData* GetShaderInterfaceData(ShaderStage shaderStage);
    virtual const PipelineShaderInfo* GetPipelineShaderInfo(ShaderStage shaderStage) const;
    virtual uint64_t GetShaderHashCode(ShaderStage stage) const;

    // Checks whether the pipeline is graphics or compute
    virtual bool IsGraphics() const { return true; }

    // Gets pipeline build info
    virtual const void* GetPipelineBuildInfo() const { return m_pPipelineInfo; }

    // Gets the mask of active shader stages bound to this pipeline
    virtual uint32_t GetShaderStageMask() const { return m_stageMask; }

    // Gets the count of active shader stages
    virtual uint32_t GetActiveShaderStageCount() const { return m_activeStageCount; }

    virtual ShaderStage GetPrevShaderStage(ShaderStage shaderStage) const;
    virtual ShaderStage GetNextShaderStage(ShaderStage shaderStage) const;

    // Checks whether tessellation off-chip mode is enabled
    virtual bool IsTessOffChip() const { return m_tessOffchip; }

    virtual bool CheckGsOnChipValidity();

    // Checks whether GS on-chip mode is enabled

    // NOTE: GS on-chip mode has different meaning for GFX6~8 and GFX9: on GFX6~8, GS on-chip mode means ES -> GS ring
    // and GS -> VS ring are both on-chip; on GFX9, ES -> GS ring is always on-chip, GS on-chip mode means GS -> VS
    // ring is on-chip.
    virtual bool IsGsOnChip() const { return m_gsOnChip; }

    // Enables GS on-chip mode
    virtual void SetGsOnChip(bool gsOnChip) { m_gsOnChip = gsOnChip; }

    virtual void DoUserDataNodeMerge();

    // Gets per pipeline options
    virtual const PipelineOptions* GetPipelineOptions() const { return &m_pPipelineInfo->options; }

    void InitShaderInfoForNullFs();

protected:
    virtual std::vector<ResourceMappingNode>* GetDummyResourceMapNodes(ShaderStage shaderStage);

    // Gets dummy vertex input create info
    virtual VkPipelineVertexInputStateCreateInfo* GetDummyVertexInputInfo() { return &m_dummyVertexInput; }

    // Gets dummy vertex binding info
    virtual std::vector<VkVertexInputBindingDescription>* GetDummyVertexBindings() { return &m_dummyVertexBindings; }

    // Gets dummy vertex attribute info
    virtual std::vector<VkVertexInputAttributeDescription>* GetDummyVertexAttributes()
        { return &m_dummyVertexAttribs; }

private:
    LLPC_DISALLOW_DEFAULT_CTOR(GraphicsContext);
    LLPC_DISALLOW_COPY_AND_ASSIGN(GraphicsContext);

    void MergeUserDataNode(uint32_t                    nodeCount1,
                           const ResourceMappingNode*  pNodes1,
                           uint32_t                    nodeCount2,
                           const ResourceMappingNode*  pNodes2,
                           uint32_t*                   pMergedNodeCount,
                           const ResourceMappingNode** ppMergedNodes);

    const GraphicsPipelineBuildInfo*    m_pPipelineInfo; // Info to build a graphics pipeline

    uint32_t m_stageMask; // Mask of active shader stages bound to this graphics pipeline
    uint32_t m_activeStageCount;    // Count of active shader stages

    ResourceUsage   m_resUsages[ShaderStageGfxCount];   // Resource usages of all graphics shader stages
    InterfaceData   m_intfData[ShaderStageGfxCount];    // Interface data of all graphics shader stages

    std::vector<ResourceMappingNode>  m_dummyResMapNodes[ShaderStageGfxCount]; // Dummy resource mapping nodes

    // Dummy vertex-specific info (for vertex shader only)
    VkPipelineVertexInputStateCreateInfo           m_dummyVertexInput;    // Dummy vertex input create info
    std::vector<VkVertexInputBindingDescription>   m_dummyVertexBindings; // Dummy vertex binding info
    std::vector<VkVertexInputAttributeDescription> m_dummyVertexAttribs;  // Dummy vertex attribute info

    bool            m_tessOffchip; // Whether to enable tessellation off-chip mode
    bool            m_gsOnChip;    // Whether to enable GS on-chip mode

    std::vector<ResourceMappingNode*>  m_allocUserDataNodes;    // Allocated user data nodes for merged shader
};

} // Llpc
