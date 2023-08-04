/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef __GRAPHICS_PIPELINE_COMMON_H__
#define __GRAPHICS_PIPELINE_COMMON_H__

#pragma once

#include "include/vk_pipeline.h"
#include "vkgcDefs.h"
#include "palColorBlendState.h"
#include "palDepthStencilState.h"
#include "palMetroHash.h"

namespace vk
{

class GraphicsPipelineLibrary;
class PipelineCache;
class RenderPass;
struct PipelineOptimizerKey;
struct GraphicsPipelineBinaryCreateInfo;
struct GraphicsPipelineShaderStageInfo;

// =====================================================================================================================
// Sample pattern structure containing pal format sample locations and sample counts
// ToDo: Move this struct to different header once render_graph implementation is removed.
struct SamplePattern
{
    Pal::MsaaQuadSamplePattern locations;
    uint32_t                   sampleCount;
};

// =====================================================================================================================
// Immediate state info that will be written during Bind() but is not
// encapsulated within a state object.
struct GraphicsPipelineObjectImmedInfo
{
    Pal::InputAssemblyStateParams         inputAssemblyState;
    Pal::TriangleRasterStateParams        triangleRasterState;
    Pal::BlendConstParams                 blendConstParams;
    Pal::DepthBiasParams                  depthBiasParams;
    Pal::DepthBoundsParams                depthBoundParams;
    Pal::PointLineRasterStateParams       pointLineRasterParams;
    Pal::LineStippleStateParams           lineStippleParams;
    Pal::ViewportParams                   viewportParams;
    Pal::ScissorRectParams                scissorRectParams;
    Pal::StencilRefMaskParams             stencilRefMasks;
    SamplePattern                         samplePattern;
    Pal::DynamicGraphicsShaderInfos       graphicsShaderInfos;
    Pal::VrsRateParams                    vrsRateParams;
    Pal::DepthStencilStateCreateInfo      depthStencilCreateInfo;
    Pal::MsaaStateCreateInfo              msaaCreateInfo;
    Pal::ColorBlendStateCreateInfo        blendCreateInfo;
    bool                                  rasterizerDiscardEnable;
    bool                                  checkDeferCompilePipeline;
    float                                 minSampleShading;
    uint32_t                              colorWriteEnable;
    uint32_t                              colorWriteMask;
    VkLogicOp                             logicOp;
    bool                                  logicOpEnable;

    // Static pipeline parameter token values.  These can be used to efficiently redundancy check static pipeline
    // state programming during pipeline binds.
    struct
    {
        uint32_t inputAssemblyState;
        uint32_t triangleRasterState;
        uint32_t pointLineRasterState;
        uint32_t lineStippleState;
        uint32_t depthBias;
        uint32_t blendConst;
        uint32_t depthBounds;
        uint32_t viewport;
        uint32_t scissorRect;
        uint32_t samplePattern;
        uint32_t fragmentShadingRate;
    } staticTokens;
};

// =====================================================================================================================
// General flags for graphics pipeline create and static state.
union GraphicsPipelineObjectFlags
{
    struct
    {
        uint32_t   bresenhamEnable           : 1;
        uint32_t   bindDepthStencilObject    : 1;
        uint32_t   bindTriangleRasterState   : 1;
        uint32_t   bindStencilRefMasks       : 1;
        uint32_t   bindInputAssemblyState    : 1;
        uint32_t   customMultiSampleState    : 1;
        uint32_t   customSampleLocations     : 1;
        uint32_t   force1x1ShaderRate        : 1;
        uint32_t   sampleShadingEnable       : 1;
        uint32_t   isPointSizeUsed           : 1;
        uint32_t   bindColorBlendObject      : 1;
        uint32_t   bindMsaaObject            : 1;
        uint32_t   viewIndexFromDeviceIndex  : 1;
        uint32_t   perpLineEndCapsEnable     : 1;
        uint32_t   shadingRateUsedInShader   : 1;
        uint32_t   fragmentShadingRateEnable : 1;
#if VKI_RAY_TRACING
        uint32_t   hasRayTracing             : 1;
        uint32_t   reserved                  : 15;
#else
        uint32_t   reserved                  : 16;
#endif
    };
    uint32_t value;
};

// =====================================================================================================================
// Creation info parameters for all the necessary state objects encapsulated
// by the Vulkan graphics pipeline.
struct GraphicsPipelineObjectCreateInfo
{
    Pal::GraphicsPipelineCreateInfo             pipeline;
    GraphicsPipelineObjectImmedInfo             immedInfo;
    uint64_t                                    staticStateMask;
    uint32_t                                    sampleCoverage;
    VkShaderStageFlagBits                       activeStages;
    VkFormat                                    dbFormat;
    uint64_t                                    dynamicStates;
#if VKI_RAY_TRACING
    uint32_t                                    dispatchRaysUserDataOffset;
#endif
    GraphicsPipelineObjectFlags                 flags;
};

// =====================================================================================================================
// Include pipeline binary information from compiler which affects information info of pipeline object
struct GraphicsPipelineBinaryInfo
{
    const PipelineOptimizerKey* pOptimizerKey;
#if VKI_RAY_TRACING
    bool                        hasRayTracing;
#endif
    bool                        hasMesh;
};

// =====================================================================================================================
// Graphics pipeline library information extracted from VkGraphicsPipelineCreateInfo
struct GraphicsPipelineLibraryInfo
{
    union
    {
        struct
        {
            uint32_t isLibrary : 1;     //> Whether the pipeline is a library or is executable
            uint32_t optimize  : 1;     //> Can do link time optimization
            uint32_t reserved  : 30;
        };
        uint32_t value;
    } flags;

    VkGraphicsPipelineLibraryFlagsEXT libFlags;     //> The sections whose state should be built via
                                                    //  VkGraphicsPipelineCreateInfo rather than copy from pipeline
                                                    //  library or be skipped.

    // The referred pipeline libraries for each section.
    const GraphicsPipelineLibrary*    pVertexInputInterfaceLib;
    const GraphicsPipelineLibrary*    pPreRasterizationShaderLib;
    const GraphicsPipelineLibrary*    pFragmentShaderLib;
    const GraphicsPipelineLibrary*    pFragmentOutputInterfaceLib;
};

// =====================================================================================================================
// The common part used by both executable graphics pipelines and graphics pipeline libraries
class GraphicsPipelineCommon : public Pipeline
{
public:
    // Create an executable graphics pipline or graphics pipeline library
    static VkResult Create(
        Device*                             pDevice,
        PipelineCache*                      pPipelineCache,
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        PipelineCreateFlags                 flags,
        const VkAllocationCallbacks*        pAllocator,
        VkPipeline*                         pPipeline);

    // Get the active shader stages through API info
    static VkShaderStageFlagBits GetActiveShaderStages(
        const VkGraphicsPipelineCreateInfo* pGraphicsPipelineCreateInfo,
        const GraphicsPipelineLibraryInfo*  pLibInfo);

    // Returns true if Dual Source Blending is to be enabled based on the given ColorBlendAttachmentState
    static bool GetDualSourceBlendEnableState(
        const Device*                              pDevice,
        const VkPipelineColorBlendStateCreateInfo* pColorBlendState,
        const Pal::ColorBlendStateCreateInfo*      pPalInfo = nullptr);

    // Returns true if src alpha is used in blending
    static bool IsSrcAlphaUsedInBlend(VkBlendFactor blend);

    // Get sample count from multisample state or render pass
    static void GetSubpassSampleCount(
        const VkPipelineMultisampleStateCreateInfo* pMs,
        const RenderPass*                           pRenderPass,
        const uint32_t                              subpass,
        uint32_t*                                   pCoverageSampleCount,
        uint32_t*                                   pColorSampleCount,
        uint32_t*                                   pDepthSampleCount);

    // Get the dynamics states specified by API info
    static uint64_t GetDynamicStateFlags(
        const VkPipelineDynamicStateCreateInfo* pDy,
        const GraphicsPipelineLibraryInfo*      pLibInfo);

    // Extract graphics pipeline library related info from VkGraphicsPipelineCreateInfo.
    static void ExtractLibraryInfo(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        PipelineCreateFlags                 flags,
        GraphicsPipelineLibraryInfo*        pLibInfo);

    // Check whether pipeline binary will be built
    static bool NeedBuildPipelineBinary(
        const GraphicsPipelineLibraryInfo* pLibInfo,
        const bool                         enableRasterization);

    static constexpr VkGraphicsPipelineLibraryFlagsEXT GraphicsPipelineLibraryAll =
        VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT    |
        VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT           |
        VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

protected:
    // Convert API information into internal create info used to create internal pipeline object
    static void BuildPipelineObjectCreateInfo(
        const Device*                          pDevice,
        const VkGraphicsPipelineCreateInfo*    pIn,
        PipelineCreateFlags                    flags,
        const GraphicsPipelineShaderStageInfo* pShaderStageInfo,
        const PipelineLayout*                  pPipelineLayout,
        const PipelineOptimizerKey*            pOptimizerKey,
        const PipelineMetadata*                pBinMeta,
        GraphicsPipelineObjectCreateInfo*      pObjInfo);

    // Populates the profile key for tuning graphics pipelines
    static void GeneratePipelineOptimizerKey(
        const Device*                          pDevice,
        const VkGraphicsPipelineCreateInfo*    pCreateInfo,
        PipelineCreateFlags                    flags,
        const GraphicsPipelineShaderStageInfo* pShaderStageInfo,
        ShaderOptimizerKey*                    pShaderKeys,
        PipelineOptimizerKey*                  pPipelineKey);

    // Generates the API PSO hash using the contents of the VkGraphicsPipelineCreateInfo struct
    static void BuildApiHash(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        PipelineCreateFlags                 flags,
        uint64_t*                           pApiHash,
        Util::MetroHash::Hash*              elfHash);

    // Generate API PSO hash for state of vertex input interface section
    static void GenerateHashForVertexInputInterfaceState(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        Util::MetroHash128*                 pBaseHasher,
        Util::MetroHash128*                 pApiHasher);

    // Generate API PSO hash for state of pre-rasterization shaders section
    static void GenerateHashForPreRasterizationShadersState(
        const VkGraphicsPipelineCreateInfo*     pCreateInfo,
        const GraphicsPipelineLibraryInfo*      pLibInfo,
        uint32_t                                dynamicStateFlags,
        Util::MetroHash128*                     pBaseHasher,
        Util::MetroHash128*                     pApiHasher);

    // Generate API PSO hash for state of fragment shader section
    static void GenerateHashForFragmentShaderState(
        const VkGraphicsPipelineCreateInfo*     pCreateInfo,
        Util::MetroHash128*                     pBaseHasher,
        Util::MetroHash128*                     pApiHasher);

    // Generate API PSO hash for state of fragment output interface section
    static void GenerateHashForFragmentOutputInterfaceState(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        Util::MetroHash128*                 pBaseHasher,
        Util::MetroHash128*                 pApiHasher);

    // Checks if rasterization is dynamically disabled
    static bool IsRasterizationDisabled(
        const VkGraphicsPipelineCreateInfo* pCreateInfo,
        const GraphicsPipelineLibraryInfo*  pLibInfo,
        uint64_t                            dynamicStateFlags);

    // Constructor of GraphicsPipelineCommon
    GraphicsPipelineCommon(
#if VKI_RAY_TRACING
        bool          hasRayTracing,
#endif
        Device* const pDevice)
        : Pipeline(
            pDevice,
#if VKI_RAY_TRACING
            hasRayTracing,
#endif
            VK_PIPELINE_BIND_POINT_GRAPHICS)
    { }
};

}

#endif/*__GRAPHICS_PIPELINE_COMMON_H__*/
