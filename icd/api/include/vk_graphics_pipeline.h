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

#ifndef __VK_GRAPHICS_PIPELINE_H__
#define __VK_GRAPHICS_PIPELINE_H__

#pragma once

#include <cmath>

#include "include/vk_device.h"
#include "include/vk_shader_code.h"
#include "include/graphics_pipeline_common.h"
#include "include/internal_mem_mgr.h"

#include "palCmdBuffer.h"
#include "palMsaaState.h"
#include "palPipeline.h"

namespace vk
{

class Device;
class PipelineCache;
class CmdBuffer;
class RenderPass;
struct CmdBufferRenderState;

// =====================================================================================================================
// Create info of graphics pipeline deferred compile
struct DeferGraphicsPipelineCreateInfo
{
    Device*                          pDevice;
    PipelineCache*                   pPipelineCache;
    GraphicsPipeline*                pPipeline;
    GraphicsPipelineBinaryCreateInfo binaryCreateInfo;
    GraphicsPipelineShaderStageInfo  shaderStageInfo;
    GraphicsPipelineObjectCreateInfo objectCreateInfo;
    Util::MetroHash::Hash            elfHash;
    ShaderOptimizerKey               shaderOptimizerKeys[ShaderStage::ShaderStageGfxCount];
    PipelineOptimizerKey             pipelineOptimizerKey;
    PipelineMetadata                 binaryMetadata;
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

    uint32_t sampleLocationsPerPixel = static_cast<uint32_t>(pSampleLocationsInfo->sampleLocationsPerPixel);

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

// =====================================================================================================================
// Force 1x1 shader rate
static void Force1x1ShaderRate(
    Pal::VrsRateParams* pVrsRateParams)
{
    pVrsRateParams->shadingRate = Pal::VrsShadingRate::_1x1;

    for (uint32 idx = 0; idx <= static_cast<uint32>(Pal::VrsCombinerStage::Image); idx++)
    {
        pVrsRateParams->combinerState[idx] = Pal::VrsCombiner::Passthrough;
    }
}

// =====================================================================================================================
// Vulkan implementation of graphics pipelines created by vkCreateGraphicsPipeline
class GraphicsPipeline final : public GraphicsPipelineCommon, public NonDispatchable<VkPipeline, GraphicsPipeline>
{
public:
    static VkResult Create(
        Device*                                 pDevice,
        PipelineCache*                          pPipelineCache,
        const VkGraphicsPipelineCreateInfo*     pCreateInfo,
        PipelineCreateFlags                     flags,
        const VkAllocationCallbacks*            pAllocator,
        VkPipeline*                             pPipeline);

    VkResult Destroy(
        Device*                         pDevice,
        const VkAllocationCallbacks*    pAllocator) override;

    const VbBindingInfo& GetVbBindingInfo() const
        { return m_vbInfo; }

    void BindToCmdBuffer(
        CmdBuffer* pCmdBuffer) const;

    const Pal::IPipeline* GetPalPipeline(uint32_t deviceIdx) const
        { return  UseOptimizedPipeline() ? m_pOptimizedPipeline[deviceIdx] : m_pPalPipeline[deviceIdx]; }

    const Pal::DynamicGraphicsShaderInfos& GetBindInfo() const { return m_info.graphicsShaderInfos; }

    const Pal::IMsaaState* const* GetMsaaStates() const { return m_pPalMsaa; }

    const Pal::MsaaQuadSamplePattern* GetSampleLocations() const
        { return &m_info.samplePattern.locations; }

     bool CustomSampleLocationsEnabled() const
         { return m_flags.customSampleLocations; }

    bool Force1x1ShaderRateEnabled() const
        { return m_flags.force1x1ShaderRate; }

    bool IsPointSizeUsed() const
        { return m_flags.isPointSizeUsed; }

    static void BindNullPipeline(CmdBuffer* pCmdBuffer);

    // Returns value of VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT
    // defined by flags member of VkGraphicsPipelineCreateInfo.
    bool ViewIndexFromDeviceIndex() const
        { return m_flags.viewIndexFromDeviceIndex; }

    GraphicsPipelineObjectFlags GetPipelineFlags() const
        { return m_flags; }
protected:
    GraphicsPipeline(
        Device* const                          pDevice,
        Pal::IPipeline**                       pPalPipeline,
        const PipelineLayout*                  pLayout,
        const GraphicsPipelineObjectImmedInfo& immedInfo,
        uint64_t                               staticStateMask,
        GraphicsPipelineObjectFlags            flags,
#if VKI_RAY_TRACING
        uint32_t                               dispatchRaysUserDataOffset,
#endif
        const VbBindingInfo&                   vbInfo,
        const PipelineInternalBufferInfo*      pInternalBuffer,
        Pal::IMsaaState**                      pPalMsaa,
        Pal::IColorBlendState**                pPalColorBlend,
        Pal::IDepthStencilState**              pPalDepthStencil,
        uint32_t                               coverageSamples,
        const Util::MetroHash::Hash&           cacheHash,
        uint64_t                               apiHash,
        Util::MetroHash64*                     pPalPipelineHasher);

    void CreateStaticState();
    void DestroyStaticState(const VkAllocationCallbacks* pAllocator);

    static VkResult CreatePipelineBinaries(
        Device*                                        pDevice,
        const VkGraphicsPipelineCreateInfo*            pCreateInfo,
        PipelineCreateFlags                            flags,
        const GraphicsPipelineShaderStageInfo*         pShaderInfo,
        const PipelineLayout*                          pPipelineLayout,
        const Util::MetroHash::Hash*                   pElfHash,
        PipelineOptimizerKey*                          pPipelineOptimizerKey,
        GraphicsPipelineBinaryCreateInfo*              pBinaryCreateInfo,
        PipelineCache*                                 pPipelineCache,
        const VkPipelineCreationFeedbackCreateInfoEXT* pCreationFeedbackInfo,
        Util::MetroHash::Hash*                         pCacheIds,
        size_t*                                        pPipelineBinarySizes,
        const void**                                   ppPipelineBinaries,
        PipelineMetadata*                              pBinaryMetadata);

    static VkResult CreatePipelineObjects(
        Device*                             pDevice,
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        PipelineCreateFlags                 flags,
        const VkAllocationCallbacks*        pAllocator,
        const PipelineLayout*               pPipelineLayout,
        const VbBindingInfo*                pVbInfo,
        const PipelineInternalBufferInfo*   pInternalBuffer,
        const size_t*                       pPipelineBinarySizes,
        const void**                        pPipelineBinaries,
        PipelineCache*                      pPipelineCache,
        const Util::MetroHash::Hash*        pCacheIds,
        uint64_t                            apiPsoHash,
        GraphicsPipelineObjectCreateInfo*   pObjectCreateInfo,
        VkPipeline*                         pPipeline);

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);

    VkResult DeferCreateOptimizedPipeline(
        Device*                           pDevice,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
        GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
        GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
        Util::MetroHash::Hash*            pElfHash);

    static VkResult CreatePalPipelineObjects(
        Device*                           pDevice,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
        const size_t*                     pPipelineBinarySizes,
        const void**                      pPipelineBinaries,
        const Util::MetroHash::Hash*      pCacheIds,
        void*                             pSystemMem,
        Pal::IPipeline**                  pPalPipeline);

    static VkResult PrepareShaderLibrary(
        Device*                           pDevice,
        const VkAllocationCallbacks*      pAllocator,
        GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
        GraphicsPipelineObjectCreateInfo* pObjectCreateInfo);

    void SetOptimizedPipeline(Pal::IPipeline** pPalPipeline);

    bool UseOptimizedPipeline() const
    {
        bool result = m_info.checkDeferCompilePipeline;
        if (result)
        {
            Util::MutexAuto pipelineSwitchLock(const_cast<Util::Mutex*>(&m_pipelineSwitchLock));
            result = m_pOptimizedPipeline[0] != nullptr && m_optimizedPipelineHash != 0;
        }
        return result;
    }
    VkResult BuildDeferCompileWorkload(
        Device*                           pDevice,
        PipelineCache*                    pPipelineCache,
        GraphicsPipelineBinaryCreateInfo* pBinaryCreateInfo,
        GraphicsPipelineShaderStageInfo*  pShaderStageInfo,
        GraphicsPipelineObjectCreateInfo* pObjectCreateInfo,
        Util::MetroHash::Hash*            pElfHash);

    static void ExecuteDeferCreateOptimizedPipeline(void* pPayload);

    GraphicsPipelineObjectImmedInfo m_info;                            // Immediate state that will go in CmdSet* functions
    Pal::IMsaaState*                m_pPalMsaa[MaxPalDevices];         // PAL MSAA state object
    Pal::IColorBlendState*          m_pPalColorBlend[MaxPalDevices];   // PAL color blend state object
    Pal::IDepthStencilState*        m_pPalDepthStencil[MaxPalDevices]; // PAL depth stencil state object
    VbBindingInfo                   m_vbInfo;                          // Information about vertex buffer bindings
    PipelineInternalBufferInfo      m_internalBufferInfo;              // Information about internal buffer
    Pal::IPipeline*                 m_pOptimizedPipeline[MaxPalDevices]; // Optimized PAL pipelines
    uint64_t                        m_optimizedPipelineHash;           // Pipeline hash of optimized PAL pipelines
    Util::Mutex                     m_pipelineSwitchLock;              // Lock for optimized pipeline and default pipeline
    DeferredCompileWorkload         m_deferWorkload;                   // Workload of deferred compiled
    GraphicsPipelineObjectFlags     m_flags;
};

} // namespace vk

#endif /* __VK_COMPUTE_PIPELINE_H__ */
