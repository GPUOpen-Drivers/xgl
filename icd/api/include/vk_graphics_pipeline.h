/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __VK_GRAPHICS_PIPELINE_H__
#define __VK_GRAPHICS_PIPELINE_H__

#pragma once

#include <cmath>

#include "include/vk_pipeline.h"
#include "include/vk_device.h"
#include "include/vk_shader_code.h"
#include "include/internal_mem_mgr.h"

#include "palCmdBuffer.h"
#include "palColorBlendState.h"
#include "palDepthStencilState.h"
#include "palMsaaState.h"
#include "palPipeline.h"

#include "llpc.h"

namespace vk
{

class Device;
class PipelineCache;
class StencilOpsCombiner;
class CmdBuffer;
struct CmdBufferRenderState;

// Sample pattern structure containing pal format sample locations and sample counts
// ToDo: Move this struct to different header once render_graph implementation is removed.
struct SamplePattern
{
    Pal::MsaaQuadSamplePattern locations;
    uint32_t                   sampleCount;
};

// Information required by the VB table manager that is defined by the graphics pipeline
struct VbBindingInfo
{
    uint32_t bindingTableSize;
    uint32_t bindingCount;

    struct
    {
        uint32_t slot;
        uint32_t byteStride;
    } bindings[Pal::MaxVertexBuffers];
};

// =====================================================================================================================
// Convert sample location coordinates from [0,1] space (sent by the application) to [-8, 7] space (accepted by PAL)
static void ConvertCoordinates(
    const VkSampleLocationEXT* pInSampleLocations,
    uint32_t                   numOfSamples,
    Pal::Offset2d*             pOutConvertedLocations)
{
    for (uint32_t s = 0; s < numOfSamples; s++)
    {
        // This maps the range [0, 1] to the range [-0.5, 0.5]
        const float shift = 0.5;
        float biasedPosX = pInSampleLocations[s].x - shift;
        float biasedPosY = pInSampleLocations[s].y - shift;

        // We use floor on the values first. Otherwise, we get round to zero behavior and -8 value is almost never used.
        // For example, without the floor, -0.5 would be the only value to map to -8. Furthermore, -0.49 would map
        // to -7, but should map to -8.
        int32_t iBiasedPosX = static_cast<int32_t>(floor(biasedPosX * Pal::SubPixelGridSize.width));
        int32_t iBiasedPosY = static_cast<int32_t>(floor(biasedPosY * Pal::SubPixelGridSize.height));

        // Sample locations are encoded in 4 bits ranging from -8 to 7. This basically divides each pixel into a
        // 16x16 grid.
        // This computation maps [-0.5, 0.5] to the range [-8, 7]
        pOutConvertedLocations[s].x = Util::Clamp<int32_t>(iBiasedPosX, -8, 7);
        pOutConvertedLocations[s].y = Util::Clamp<int32_t>(iBiasedPosY, -8, 7);
    }
}

// =====================================================================================================================
// Convert VkSampleLocationsInfoEXT into Pal::MsaaQuadSamplePattern
static void ConvertToPalMsaaQuadSamplePattern(
    const VkSampleLocationsInfoEXT* pSampleLocationsInfo,
    Pal::MsaaQuadSamplePattern*     pLocations)
{
    uint32_t gridWidth  = pSampleLocationsInfo->sampleLocationGridSize.width;
    uint32_t gridHeight = pSampleLocationsInfo->sampleLocationGridSize.height;

    VK_ASSERT(gridWidth * gridHeight != 0);

    uint32_t sampleLocationsPerPixel = (uint32_t)pSampleLocationsInfo->sampleLocationsPerPixel;

    // Sample locations are passed in the [0, 1] range. We need to convert them to [-8, 7]
    // discrete range for setting the registers.
    for (uint32_t y = 0; y < Pal::MaxGridSize.height; y++)
    {
        for (uint32_t x = 0; x < Pal::MaxGridSize.width; x++)
        {
            const uint32_t xOffset = x % gridWidth;
            const uint32_t yOffset = y % gridHeight;

            const uint32_t pixelOffset = (yOffset * gridWidth + xOffset) * sampleLocationsPerPixel;

            Pal::Offset2d* pSamplePattern = nullptr;

            if ((x == 0) && (y == 0))
            {
                pSamplePattern = pLocations->topLeft;
            }
            else if ((x == 1) && (y == 0))
            {
                pSamplePattern = pLocations->topRight;
            }
            else if ((x == 0) && (y == 1))
            {
                pSamplePattern = pLocations->bottomLeft;
            }
            else if ((x == 1) && (y == 1))
            {
                pSamplePattern = pLocations->bottomRight;
            }

            ConvertCoordinates(
                &pSampleLocationsInfo->pSampleLocations[pixelOffset],
                sampleLocationsPerPixel,
                pSamplePattern);
        }
    }
}

bool GetDualSourceBlendEnableState(const VkPipelineColorBlendAttachmentState& pColorBlendAttachmentState);

// =====================================================================================================================
// Vulkan implementation of graphics pipelines created by vkCreateGraphicsPipeline
class GraphicsPipeline : public Pipeline, public NonDispatchable<VkPipeline, GraphicsPipeline>
{
public:
    static VkResult Create(
        Device*                                 pDevice,
        PipelineCache*                          pPipelineCache,
        const VkGraphicsPipelineCreateInfo*     pCreateInfo,
        const VkAllocationCallbacks*            pAllocator,
        VkPipeline*                             pPipeline);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    const VbBindingInfo* GetVbBindingInfo() const
        { return &m_vbInfo; }

    void BindToCmdBuffer(
        CmdBuffer*                             pCmdBuffer,
        CmdBufferRenderState*                  pRenderState,
        StencilOpsCombiner*                    pStencilCombiner) const;

    void BindToCmdBuffer(
        CmdBuffer*                             pCmdBuffer,
        CmdBufferRenderState*                  pRenderState,
        StencilOpsCombiner*                    pStencilCombiner,
        const Pal::DynamicGraphicsShaderInfos& graphicsShaderInfos) const;

    const Pal::IMsaaState* const* GetMsaaStates() const { return m_pPalMsaa; }

    static void BindNullPipeline(CmdBuffer* pCmdBuffer);

    // This function returns true if any of the bits in the given state mask (corresponding to shifted values of
    // VK_DYNAMIC_STATE_*) should be programmed by the pipeline when it is bound (instead of by the application via
    // vkCmdSet*).
    VK_INLINE bool PipelineSetsState(DynamicStatesInternal dynamicState) const
    {
        return ((m_info.staticStateMask & (1UL << static_cast<uint32_t>(dynamicState))) != 0);
    }

    // Returns value of VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
    // defined by flags member of VkGraphicsPipelineCreateInfo.
    bool ViewIndexFromDeviceIndex() const
    {
        return m_flags.viewIndexFromDeviceIndex;
    }

protected:
    // Immediate state info that will be written during Bind() but is not
    // encapsulated within a state object.
    //
    // NOTE: This structure needs to be revisited when the new PAL state headers
    // are in place.
    struct ImmedInfo
    {
        // Input to >CmdSetInputAssemblyState:
        Pal::InputAssemblyStateParams  inputAssemblyState;

        // Input to CmdSetTriangleRasterState:
        Pal::TriangleRasterStateParams triangleRasterState;

        // Bitfield to detect which subset of pipeline state is static (written at bind-time).
        uint32_t                        staticStateMask;

        Pal::BlendConstParams                 blendConstParams;
        Pal::DepthBiasParams                  depthBiasParams;
        Pal::DepthBoundsParams                depthBoundParams;
        Pal::PointLineRasterStateParams       pointLineRasterParams;
        Pal::ViewportParams                   viewportParams;
        Pal::ScissorRectParams                scissorRectParams;
        Pal::StencilRefMaskParams             stencilRefMasks;
        SamplePattern                         samplePattern;
        Pal::DynamicGraphicsShaderInfos       graphicsWaveLimitParams;

        // Static pipeline parameter token values.  These can be used to efficiently redundancy check static pipeline
        // state programming during pipeline binds.
        struct
        {
            uint32_t inputAssemblyState;
            uint32_t triangleRasterState;
            uint32_t pointLineRasterState;
            uint32_t depthBias;
            uint32_t blendConst;
            uint32_t depthBounds;
            uint32_t viewport;
            uint32_t scissorRect;
            uint32_t samplePattern;
            uint32_t waveLimits;
        } staticTokens;
    };

    GraphicsPipeline(
        Device* const                          pDevice,
        Pal::IPipeline**                       pPalPipeline,
        const PipelineLayout*                  pLayout,
        const ImmedInfo&                       immedInfo,
        const VbBindingInfo&                   vbInfo,
        Pal::IMsaaState**                      pPalMsaa,
        Pal::IColorBlendState**                pPalColorBlend,
        Pal::IDepthStencilState**              pPalDepthStencil,
        uint32_t                               coverageSamples,
        bool                                   viewIndexFromDeviceIndex,
        PipelineBinaryInfo*                    pBinary,
        uint64_t                               apiHash);

    void CreateStaticState();
    void DestroyStaticState(const VkAllocationCallbacks* pAllocator);

    ~GraphicsPipeline();

    // Creation info parameters for all the necessary PAL/LLPC/SCPC state objects encapsulated
    // by the Vulkan graphics pipeline.
    struct CreateInfo
    {
        Pal::GraphicsPipelineCreateInfo             pipeline;
        Pal::MsaaStateCreateInfo                    msaa;
        Pal::ColorBlendStateCreateInfo              blend;
        Pal::DepthStencilStateCreateInfo            ds;
        ImmedInfo                                   immedInfo;
        const PipelineLayout*                       pLayout;
        uint32_t                                    sampleCoverage;
        VkShaderStageFlagBits                       activeStages;
        uint32_t                                    rasterizationStream;
    };

    static void ConvertGraphicsPipelineInfo(
        Device*                             pDevice,
        const VkGraphicsPipelineCreateInfo* pIn,
        const VbBindingInfo*                pVbInfo,
        CreateInfo*                         pInfo);

    static void BuildRasterizationState(
        Device*                                       pDevice,
        const VkPipelineRasterizationStateCreateInfo* pIn,
        CreateInfo*                                   pInfo,
        const bool                                    dynamicStateFlags[]);

    static void GenerateHashFromVertexInputStateCreateInfo(
        Util::MetroHash128*                         pHasher,
        const VkPipelineVertexInputStateCreateInfo& desc);

    static void GenerateHashFromInputAssemblyStateCreateInfo(
        Util::MetroHash128*                           pBaseHasher,
        Util::MetroHash128*                           pApiHasher,
        const VkPipelineInputAssemblyStateCreateInfo& desc);

    static void GenerateHashFromTessellationStateCreateInfo(
        Util::MetroHash128*                          pHasher,
        const VkPipelineTessellationStateCreateInfo& desc);

    static void GenerateHashFromViewportStateCreateInfo(
        Util::MetroHash128*                      pHasher,
        const VkPipelineViewportStateCreateInfo& desc);

    static void GenerateHashFromRasterizationStateCreateInfo(
        Util::MetroHash128*                           pBaseHasher,
        Util::MetroHash128*                           pApiHasher,
        const VkPipelineRasterizationStateCreateInfo& desc);

    static void GenerateHashFromMultisampleStateCreateInfo(
        Util::MetroHash128*                         pBaseHasher,
        Util::MetroHash128*                         pApiHasher,
        const VkPipelineMultisampleStateCreateInfo& desc);

    static void GenerateHashFromDepthStencilStateCreateInfo(
        Util::MetroHash128*                          pHasher,
        const VkPipelineDepthStencilStateCreateInfo& desc);

    static void GenerateHashFromColorBlendStateCreateInfo(
        Util::MetroHash128*                        pBaseHasher,
        Util::MetroHash128*                        pApiHasher,
        const VkPipelineColorBlendStateCreateInfo& desc);

    static void GenerateHashFromDynamicStateCreateInfo(
        Util::MetroHash128*                     pHasher,
        const VkPipelineDynamicStateCreateInfo& desc);

    static uint64_t BuildApiHash(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        Util::MetroHash::Hash*              pBaseHash);

private:
    ImmedInfo                 m_info;                             // Immediate state that will go in CmdSet* functions
    Pal::IMsaaState*          m_pPalMsaa[MaxPalDevices];          // PAL MSAA state object
    Pal::IColorBlendState*    m_pPalColorBlend[MaxPalDevices];    // PAL color blend state object
    Pal::IDepthStencilState*  m_pPalDepthStencil[MaxPalDevices];  // PAL depth stencil state object
    VbBindingInfo             m_vbInfo;                           // Information about vertex buffer bindings

    uint32_t                  m_coverageSamples;

    union
    {
        uint8_t value;
        struct
        {
            uint8_t viewIndexFromDeviceIndex : 1;
            uint8_t reserved                 : 7;
        };
    } m_flags;
};

} // namespace vk

#endif /* __VK_COMPUTE_PIPELINE_H__ */
