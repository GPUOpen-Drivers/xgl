/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_GRAPHICS_PIPELINE_LIBRARY_H__
#define __VK_GRAPHICS_PIPELINE_LIBRARY_H__

#pragma once

#include "include/graphics_pipeline_common.h"
#include "include/vk_pipeline_cache.h"

namespace vk
{

class GraphicsPipelineLibrary final : public GraphicsPipelineCommon, public NonDispatchable<VkPipeline, GraphicsPipelineLibrary>
{
public:
    static VkResult Create(
        Device*                             pDevice,
        PipelineCache*                      pPipelineCache,
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        const GraphicsPipelineExtStructs&   extStructs,
        VkPipelineCreateFlags2KHR           flags,
        uint32_t                            internalFlags,
        const VkAllocationCallbacks*        pAllocator,
        VkPipeline*                         pPipeline);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    const GraphicsPipelineObjectCreateInfo& GetPipelineObjectCreateInfo() const
        { return m_objectCreateInfo; }

    const GraphicsPipelineBinaryCreateInfo& GetPipelineBinaryCreateInfo() const
        { return *m_pBinaryCreateInfo; }

    VkGraphicsPipelineLibraryFlagsEXT GetLibraryFlags() const
        { return m_pBinaryCreateInfo->libFlags; }

    uint64_t GetDynamicStates() const
        { return m_objectCreateInfo.dynamicStates; }

    const Util::MetroHash::Hash* GetElfHash() const
        { return &m_elfHash; }

    void GetOwnedPalShaderLibraries(const Pal::IShaderLibrary* pLibraries[GraphicsLibraryCount]) const;

    void SetAltLibrary(GraphicsPipelineLibrary* pLibrary) { m_altLibrary = pLibrary; }

    GraphicsPipelineLibrary* GetAltLibrary() const { return m_altLibrary; }
private:
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipelineLibrary);

    GraphicsPipelineLibrary(
        Device*                                 pDevice,
        const GraphicsPipelineObjectCreateInfo& objectInfo,
        const GraphicsPipelineBinaryCreateInfo* pBinaryInfo,
        const GraphicsPipelineLibraryInfo&      libInfo,
        const Util::MetroHash::Hash&            elfHash,
        const uint64_t                          apiHash,
        const GplModuleState*                   pGplModuleStates,
        const PipelineLayout*                   pPipelineLayout);

    static VkResult CreatePartialPipelineBinary(
        const Device*                          pDevice,
        PipelineCache*                         pPipelineCache,
        const VkGraphicsPipelineCreateInfo*    pCreateInfo,
        const GraphicsPipelineLibraryInfo*     pLibInfo,
        const GraphicsPipelineShaderStageInfo* pShaderStageInfo,
        GraphicsPipelineBinaryCreateInfo*      pBinaryCreateInfo,
        const VkAllocationCallbacks*           pAllocator,
        GplModuleState*                        pTempModuleStages);

    const GraphicsPipelineObjectCreateInfo  m_objectCreateInfo;
    const GraphicsPipelineBinaryCreateInfo* m_pBinaryCreateInfo;
    const GraphicsPipelineLibraryInfo       m_libInfo;
    GplModuleState                          m_gplModuleStates[ShaderStage::ShaderStageGfxCount];
    const Util::MetroHash::Hash             m_elfHash;
    GraphicsPipelineLibrary*                m_altLibrary;
};

}

#endif/*__VK_GRAPHICS_PIPELINE_LIBRARY_H__*/
