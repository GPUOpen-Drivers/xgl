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

#include "include/app_shader_optimizer.h"
#include "include/vk_cmdbuffer.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_graphics_pipeline_library.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"
#include "include/vk_shader.h"

#include "palVectorImpl.h"
#include "palElfReader.h"

namespace vk
{

// We need to make sure that the number of dynamic states should not be larger than 64.
// Otherwise, we cannot represent the collection of them by a uint32.
static_assert(static_cast<uint32_t>(DynamicStatesInternal::DynamicStatesInternalCount) <= 64,
              "Unexpected enum count: DynamicStatesInternal");

// =====================================================================================================================
// The dynamic states of Vertex Input Interface section
// - VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY
// - VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE
// - VK_DYNAMIC_STATE_VERTEX_INPUT_EXT
// - VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE
constexpr uint64_t ViiDynamicStatesMask = 0
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopology))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStride))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInput));

// =====================================================================================================================
// The dynamic states of Pre-Rasterization Shaders section
// - VK_DYNAMIC_STATE_VIEWPORT
// - VK_DYNAMIC_STATE_SCISSOR
// - VK_DYNAMIC_STATE_LINE_WIDTH
// - VK_DYNAMIC_STATE_DEPTH_BIAS
// - VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV (not available)
// - VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV (not available)
// - VK_DYNAMIC_STATE_LINE_STIPPLE_EXT
// - VK_DYNAMIC_STATE_CULL_MODE
// - VK_DYNAMIC_STATE_FRONT_FACE
// - VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT
// - VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
// - VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT (not available)
// - VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE
// - VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE
// - VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT (not available)
// - VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_POLYGON_MODE_EXT (not available)
// - VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT (not available)
// - VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT (not available)
// - VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT (not available)
// - VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT (not available)
// - VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT (not available)
// - VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT (not available)
constexpr uint64_t PrsDynamicStatesMask = 0
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Viewport))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Scissor))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineWidth))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBias))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStipple))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::CullMode))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::FrontFace))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::TessellationDomainOrigin))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClampEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PolygonMode))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizationStream))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ConservativeRasterizationMode))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ExtraPrimitiveOverestimationSize))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ProvokingVertexMode))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineRasterizationMode))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStippleEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipNegativeOneToOne));

// =====================================================================================================================
// The dynamic states of Fragment Shader (Post-Rasterization) section
// - VK_DYNAMIC_STATE_DEPTH_BOUNDS
// - VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
// - VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
// - VK_DYNAMIC_STATE_STENCIL_REFERENCE
// - VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR
// - VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
// - VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
// - VK_DYNAMIC_STATE_DEPTH_COMPARE_OP
// - VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE
// - VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE
// - VK_DYNAMIC_STATE_STENCIL_OP
constexpr uint64_t FgsDynamicStatesMask = 0
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilReference))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOp))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilOp));

// =====================================================================================================================
// The dynamic states of Fragment Output Interface section
// - VK_DYNAMIC_STATE_BLEND_CONSTANTS
// - VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT
// - VK_DYNAMIC_STATE_LOGIC_OP_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT
// - VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT (not available)
// - VK_DYNAMIC_STATE_SAMPLE_MASK_EXT (not available)
// - VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT (not available)
// - VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT (not available)
constexpr uint64_t FoiDynamicStatesMask = 0
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocations))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOp))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizationSamples))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleMask))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::AlphaToCoverageEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::AlphaToOneEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOpEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEnable))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEquation))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteMask))
    | (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsEnable));

// =====================================================================================================================
// Helper function used to check whether a specific dynamic state is set
static bool IsDynamicStateEnabled(const uint64_t dynamicStateFlags, const DynamicStatesInternal internalState)
{
    return dynamicStateFlags & (1ULL << static_cast<uint32_t>(internalState));
}

// =====================================================================================================================
// Returns true if the given VkBlendFactor factor is a dual source blend factor
static bool IsDualSourceBlend(VkBlendFactor blend)
{
    switch (blend)
    {
    case VK_BLEND_FACTOR_SRC1_COLOR:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
    case VK_BLEND_FACTOR_SRC1_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        return true;
    default:
        return false;
    }
}

// =====================================================================================================================
static void BuildPalColorBlendStateCreateInfo(
    const VkPipelineColorBlendStateCreateInfo* pColorBlendState,
    const uint64_t                             dynamicStateFlags,
    Pal::ColorBlendStateCreateInfo*            pInfo)
{
    const uint32_t numColorTargets = Min(pColorBlendState->attachmentCount, Pal::MaxColorTargets);

    for (uint32_t i = 0; i < numColorTargets; ++i)
    {
        const VkPipelineColorBlendAttachmentState& attachmentState = pColorBlendState->pAttachments[i];
        auto pBlendDst = &pInfo->targets[i];

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable) == false)
        {
            pBlendDst->blendEnable = (attachmentState.blendEnable == VK_TRUE);
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation) == false)
        {
            pBlendDst->srcBlendColor  = VkToPalBlend(attachmentState.srcColorBlendFactor);
            pBlendDst->dstBlendColor  = VkToPalBlend(attachmentState.dstColorBlendFactor);
            pBlendDst->blendFuncColor = VkToPalBlendFunc(attachmentState.colorBlendOp);
            pBlendDst->srcBlendAlpha  = VkToPalBlend(attachmentState.srcAlphaBlendFactor);
            pBlendDst->dstBlendAlpha  = VkToPalBlend(attachmentState.dstAlphaBlendFactor);
            pBlendDst->blendFuncAlpha = VkToPalBlendFunc(attachmentState.alphaBlendOp);
        }
    }
}

// =====================================================================================================================
bool GraphicsPipelineCommon::GetDualSourceBlendEnableState(
    const Device*                              pDevice,
    const VkPipelineColorBlendStateCreateInfo* pColorBlendState,
    const Pal::ColorBlendStateCreateInfo*      pPalInfo)
{
    bool dualSourceBlend = false;

    bool canEnableDualSourceBlend;
    if (pPalInfo != nullptr)
    {
        canEnableDualSourceBlend = pDevice->PalDevice(DefaultDeviceIndex)->CanEnableDualSourceBlend(*pPalInfo);
    }
    else
    {
        Pal::ColorBlendStateCreateInfo palInfo = {};
        BuildPalColorBlendStateCreateInfo(pColorBlendState, 0, &palInfo);
        canEnableDualSourceBlend = pDevice->PalDevice(DefaultDeviceIndex)->CanEnableDualSourceBlend(palInfo);
    }

    if (canEnableDualSourceBlend)
    {
        const uint32_t numColorTargets = Min(pColorBlendState->attachmentCount, Pal::MaxColorTargets);
        for (uint32_t i = 0; (i < numColorTargets) && (dualSourceBlend == false); ++i)
        {
            const VkPipelineColorBlendAttachmentState& attachmentState = pColorBlendState->pAttachments[i];

            bool attachmentEnabled = false;

            if (attachmentState.blendEnable == VK_TRUE)
            {
                attachmentEnabled |= IsDualSourceBlend(attachmentState.srcAlphaBlendFactor);
                attachmentEnabled |= IsDualSourceBlend(attachmentState.dstAlphaBlendFactor);
                attachmentEnabled |= IsDualSourceBlend(attachmentState.srcColorBlendFactor);
                attachmentEnabled |= IsDualSourceBlend(attachmentState.dstColorBlendFactor);
            }

            dualSourceBlend |= attachmentEnabled;
        }
    }

    return dualSourceBlend;
}

// =====================================================================================================================
bool GraphicsPipelineCommon::IsSrcAlphaUsedInBlend(VkBlendFactor blend)
{
    switch (blend)
    {
    case VK_BLEND_FACTOR_SRC_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
    case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
    case VK_BLEND_FACTOR_SRC1_ALPHA:
    case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
        return true;
    default:
        return false;
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GetSubpassSampleCount(
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    uint32_t*                                   pCoverageSampleCount,
    uint32_t*                                   pColorSampleCount,
    uint32_t*                                   pDepthSampleCount)
{
    const uint32_t rasterizationSampleCount   = pMs->rasterizationSamples;

    uint32_t coverageSampleCount = (pRenderPass != VK_NULL_HANDLE) ?
        pRenderPass->GetSubpassMaxSampleCount(subpass) : rasterizationSampleCount;

    // subpassCoverageSampleCount would be equal to zero if there are zero attachments.
    coverageSampleCount = (coverageSampleCount == 0) ? rasterizationSampleCount : coverageSampleCount;

    if (pCoverageSampleCount != nullptr)
    {
        *pCoverageSampleCount = coverageSampleCount;
    }

    // In case we are rendering to color only, we make sure to set the DepthSampleCount to CoverageSampleCount.
    // CoverageSampleCount is really the ColorSampleCount in this case. This makes sure we have a consistent
    // sample count and that we get correct MSAA behavior.
    // Similar thing for when we are rendering to depth only. The expectation in that case is that all
    // sample counts should match.
    // This shouldn't interfere with EQAA. For EQAA, if ColorSampleCount is not equal to DepthSampleCount
    // and they are both greater than one, then we do not force them to match.

    if (pColorSampleCount != nullptr)
    {
        uint32_t colorSampleCount = (pRenderPass != VK_NULL_HANDLE) ?
            pRenderPass->GetSubpassColorSampleCount(subpass) : rasterizationSampleCount;

        colorSampleCount = (colorSampleCount == 0) ? coverageSampleCount : colorSampleCount;

        *pColorSampleCount = colorSampleCount;
    }

    if (pDepthSampleCount != nullptr)
    {
        uint32_t depthSampleCount = (pRenderPass != VK_NULL_HANDLE) ?
            pRenderPass->GetSubpassDepthSampleCount(subpass) : rasterizationSampleCount;

        depthSampleCount = (depthSampleCount == 0) ? coverageSampleCount : depthSampleCount;

        *pDepthSampleCount = depthSampleCount;
    }
}

// =====================================================================================================================
static VkFormat GetDepthFormat(
    const RenderPass*                       pRenderPass,
    const uint32_t                          subpassIndex,
    const VkPipelineRenderingCreateInfoKHR* pPipelineRenderingCreateInfoKHR
    )
{
    VkFormat format = VK_FORMAT_UNDEFINED;

    if (pRenderPass != nullptr)
    {
        format = pRenderPass->GetDepthStencilAttachmentFormat(subpassIndex);
    }
    else if (pPipelineRenderingCreateInfoKHR != nullptr)
    {
        format = (pPipelineRenderingCreateInfoKHR->depthAttachmentFormat != VK_FORMAT_UNDEFINED) ?
                   pPipelineRenderingCreateInfoKHR->depthAttachmentFormat :
                   pPipelineRenderingCreateInfoKHR->stencilAttachmentFormat;
    }

    return format;
}

// =====================================================================================================================
static uint32_t GetColorAttachmentCount(
    const RenderPass*                       pRenderPass,
    const uint32_t                          subpassIndex,
    const VkPipelineRenderingCreateInfoKHR* pPipelineRenderingCreateInfoKHR
)
{
    return (pRenderPass != nullptr) ? pRenderPass->GetSubpassColorReferenceCount(subpassIndex) :
           (pPipelineRenderingCreateInfoKHR != nullptr) ? pPipelineRenderingCreateInfoKHR->colorAttachmentCount :
           0u;
}

// =====================================================================================================================
static VkShaderStageFlagBits GetLibraryActiveShaderStages(
    const VkGraphicsPipelineLibraryFlagsEXT libFlags)
{
    constexpr VkShaderStageFlagBits PrsActiveStageMask =
        static_cast<VkShaderStageFlagBits>(
                                           VK_SHADER_STAGE_TASK_BIT_EXT |
                                           VK_SHADER_STAGE_MESH_BIT_EXT |
                                           VK_SHADER_STAGE_VERTEX_BIT |
                                           VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                           VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                           VK_SHADER_STAGE_GEOMETRY_BIT);
    constexpr VkShaderStageFlagBits FgsActiveStageMask =
        static_cast<VkShaderStageFlagBits>(VK_SHADER_STAGE_FRAGMENT_BIT);

    VkShaderStageFlagBits activeStageMask = static_cast<VkShaderStageFlagBits>(0);

    if (libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        activeStageMask = static_cast<VkShaderStageFlagBits>(activeStageMask | PrsActiveStageMask);
    }
    if (libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
    {
        activeStageMask = static_cast<VkShaderStageFlagBits>(activeStageMask | FgsActiveStageMask);
    }

    return activeStageMask;
}

// =====================================================================================================================
VkShaderStageFlagBits GraphicsPipelineCommon::GetActiveShaderStages(
    const VkGraphicsPipelineCreateInfo* pGraphicsPipelineCreateInfo,
    const GraphicsPipelineLibraryInfo*  pLibInfo)
{
    VK_ASSERT(pGraphicsPipelineCreateInfo != nullptr);

    VkShaderStageFlagBits activeStages = static_cast<VkShaderStageFlagBits>(0);

    for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
    {
        activeStages = static_cast<VkShaderStageFlagBits>(activeStages | pGraphicsPipelineCreateInfo->pStages[i].stage);
    }

    VkShaderStageFlagBits activeStageMask = GetLibraryActiveShaderStages(pLibInfo->libFlags);

    activeStages = static_cast<VkShaderStageFlagBits>(activeStages & activeStageMask);

    const GraphicsPipelineLibrary* libraries[] = { pLibInfo->pPreRasterizationShaderLib, pLibInfo->pFragmentShaderLib };

    for (uint32_t i = 0; i < Util::ArrayLen(libraries); ++i)
    {
        if (libraries[i] != nullptr)
        {
            const VkShaderStageFlagBits libShaderStages =
                libraries[i]->GetPipelineObjectCreateInfo().activeStages;

            const VkShaderStageFlagBits libActiveStageMask =
                GetLibraryActiveShaderStages(libraries[i]->GetLibraryFlags());

            activeStages = static_cast<VkShaderStageFlagBits>(activeStages | (libActiveStageMask & libShaderStages));
        }
    }

    activeStageMask = static_cast<VkShaderStageFlagBits>(0);

    if (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        activeStageMask = static_cast<VkShaderStageFlagBits>(activeStageMask |
                                                             VK_SHADER_STAGE_TASK_BIT_EXT |
                                                             VK_SHADER_STAGE_MESH_BIT_EXT |
                                                             VK_SHADER_STAGE_VERTEX_BIT |
                                                             VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
                                                             VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                                             VK_SHADER_STAGE_GEOMETRY_BIT);
    }
    if (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
    {
        activeStageMask = static_cast<VkShaderStageFlagBits>(activeStageMask | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    activeStages = static_cast<VkShaderStageFlagBits>(activeStages & activeStageMask);

    return activeStages;
}

// =====================================================================================================================
uint64_t GraphicsPipelineCommon::GetDynamicStateFlags(
    const VkPipelineDynamicStateCreateInfo* pDy,
    const GraphicsPipelineLibraryInfo*      pLibInfo)
{
    uint64_t dynamicState = 0;

    if (pLibInfo->pVertexInputInterfaceLib != nullptr)
    {
        const uint64_t libDynamicStates =
            ViiDynamicStatesMask & pLibInfo->pVertexInputInterfaceLib->GetDynamicStates();

        dynamicState |= libDynamicStates;
    }

    if (pLibInfo->pPreRasterizationShaderLib != nullptr)
    {
        const uint64_t libDynamicStates =
            PrsDynamicStatesMask & pLibInfo->pPreRasterizationShaderLib->GetDynamicStates();

        dynamicState |= libDynamicStates;
    }

    if (pLibInfo->pFragmentShaderLib != nullptr)
    {
        const uint64_t libDynamicStates =
            FgsDynamicStatesMask & pLibInfo->pFragmentShaderLib->GetDynamicStates();

        dynamicState |= libDynamicStates;
    }

    if (pLibInfo->pFragmentOutputInterfaceLib != nullptr)
    {
        const uint64_t libDynamicStates =
            FoiDynamicStatesMask & pLibInfo->pFragmentOutputInterfaceLib->GetDynamicStates();

        dynamicState |= libDynamicStates;
    }

    // The section of the following dynamic states are not defined, so we don't get them from libraries
    // - VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV (not available)
    // - VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT (not available)
    // - VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV (not available)

    // Get dynamic states from VkPipelineDynamicStateCreateInfo
    if (pDy != nullptr)
    {
        const uint64_t viiMask =
            (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)    ? UINT64_MAX : 0;
        const uint64_t prsMask =
            (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ? UINT64_MAX : 0;
        const uint64_t fgsMask =
            (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)           ? UINT64_MAX : 0;
        const uint64_t foiMask =
            (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) ? UINT64_MAX : 0;

        for (uint32_t i = 0; i < pDy->dynamicStateCount; ++i)
        {
            switch (static_cast<uint32_t>(pDy->pDynamicStates[i]))
            {
            case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY:
                dynamicState |= viiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopology));
                break;
            case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE:
                dynamicState |= viiMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStride));
                break;
            case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE:
                dynamicState |= viiMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnable));
                break;
            case VK_DYNAMIC_STATE_VERTEX_INPUT_EXT:
                dynamicState |= viiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInput));
                break;
            case VK_DYNAMIC_STATE_VIEWPORT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Viewport));
                break;
            case VK_DYNAMIC_STATE_SCISSOR:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Scissor));
                break;
            case VK_DYNAMIC_STATE_LINE_WIDTH:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineWidth));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BIAS:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBias));
                break;
            case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStipple));
                break;
            case VK_DYNAMIC_STATE_CULL_MODE:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::CullMode));
                break;
            case VK_DYNAMIC_STATE_FRONT_FACE:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::FrontFace));
                break;
            case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount));
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Viewport));
                break;
            case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount));
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Scissor));
                break;
            case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnable));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnable));
                break;
            case VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::TessellationDomainOrigin));
                break;
            case VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClampEnable));
                break;
            case VK_DYNAMIC_STATE_POLYGON_MODE_EXT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::PolygonMode));
                break;
            case VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizationStream));
                break;
            case VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ConservativeRasterizationMode));
                break;
            case VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ExtraPrimitiveOverestimationSize));
                break;
            case VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipEnable));
                break;
            case VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ProvokingVertexMode));
                break;
            case VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineRasterizationMode));
                break;
            case VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT:
                dynamicState |= prsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStippleEnable));
                break;
            case VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT:
                dynamicState |= prsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipNegativeOneToOne));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds));
                break;
            case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask));
                break;
            case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask));
                break;
            case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilReference));
                break;
            case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
                dynamicState |= fgsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr));
                break;
            case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnable));
                break;
            case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnable));
                break;
            case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOp));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE:
                dynamicState |= fgsMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnable));
                break;
            case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnable));
                break;
            case VK_DYNAMIC_STATE_STENCIL_OP:
                dynamicState |= fgsMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilOp));
                break;
            case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants));
                break;
            case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocations));
                break;
            case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnable));
                break;
            case VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT:
                dynamicState |= foiMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizationSamples));
                break;
            case VK_DYNAMIC_STATE_SAMPLE_MASK_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleMask));
                break;
            case VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT:
                dynamicState |= foiMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::AlphaToCoverageEnable));
                break;
            case VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::AlphaToOneEnable));
                break;
            case VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOpEnable));
                break;
            case VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEnable));
                break;
            case VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEquation));
                break;
            case VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteMask));
                break;
            case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT:
                dynamicState |= foiMask &
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsEnable));
                break;
            case VK_DYNAMIC_STATE_LOGIC_OP_EXT:
                dynamicState |= foiMask & (1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOp));
                break;
            default:
                VK_ASSERT(!"Unknown dynamic state");
                break;
            }
        }
    }

    return dynamicState;
}

// =====================================================================================================================
void GraphicsPipelineCommon::ExtractLibraryInfo(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    PipelineCreateFlags                 flags,
    GraphicsPipelineLibraryInfo*        pLibInfo)
{

    EXTRACT_VK_STRUCTURES_1(
        gfxPipeline,
        GraphicsPipelineLibraryCreateInfoEXT,
        PipelineLibraryCreateInfoKHR,
        static_cast<const VkGraphicsPipelineLibraryCreateInfoEXT*>(pCreateInfo->pNext),
        GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT,
        PIPELINE_LIBRARY_CREATE_INFO_KHR)

    pLibInfo->flags.isLibrary = (flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR) ? 1 : 0;

    pLibInfo->flags.optimize  = (flags & VK_PIPELINE_CREATE_LINK_TIME_OPTIMIZATION_BIT_EXT) ? 1 : 0;

    pLibInfo->libFlags =
        (pLibInfo->flags.isLibrary == false) ? GraphicsPipelineLibraryAll :
        (pGraphicsPipelineLibraryCreateInfoEXT == nullptr) ? 0 : pGraphicsPipelineLibraryCreateInfoEXT->flags;

    pLibInfo->pVertexInputInterfaceLib    = nullptr;
    pLibInfo->pPreRasterizationShaderLib  = nullptr;
    pLibInfo->pFragmentShaderLib          = nullptr;
    pLibInfo->pFragmentOutputInterfaceLib = nullptr;

    if (pPipelineLibraryCreateInfoKHR != nullptr)
    {
        for (uint32_t i = 0; i < pPipelineLibraryCreateInfoKHR->libraryCount; ++i)
        {
            const GraphicsPipelineLibrary* pPipelineLib =
                GraphicsPipelineLibrary::ObjectFromHandle(pPipelineLibraryCreateInfoKHR->pLibraries[i]);

            if (pPipelineLib != nullptr)
            {
                VkGraphicsPipelineLibraryFlagsEXT linkLibFlags = pPipelineLib->GetLibraryFlags();

                if (linkLibFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
                {
                    VK_ASSERT(pLibInfo->pVertexInputInterfaceLib == nullptr);
                    pLibInfo->pVertexInputInterfaceLib = pPipelineLib;
                    pLibInfo->libFlags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;
                }

                if (linkLibFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
                {
                    VK_ASSERT(pLibInfo->pPreRasterizationShaderLib == nullptr);
                    pLibInfo->pPreRasterizationShaderLib = pPipelineLib;
                    pLibInfo->libFlags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;
                }

                if (linkLibFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
                {
                    VK_ASSERT(pLibInfo->pFragmentShaderLib == nullptr);
                    pLibInfo->pFragmentShaderLib = pPipelineLib;
                    pLibInfo->libFlags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;
                }

                if (linkLibFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
                {
                    VK_ASSERT(pLibInfo->pFragmentOutputInterfaceLib == nullptr);
                    pLibInfo->pFragmentOutputInterfaceLib = pPipelineLib;
                    pLibInfo->libFlags &= ~VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
                }
            }
        }
    }
}

// =====================================================================================================================
bool GraphicsPipelineCommon::NeedBuildPipelineBinary(
    const GraphicsPipelineLibraryInfo* pLibInfo,
    const bool                         enableRasterization)
{
    bool result = false;

    if (pLibInfo->flags.isLibrary == false)
    {
        result = true;
    }
    else if (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        result = true;
    }
    else if ((enableRasterization == true) &&
             (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT))
    {
        result = true;
    }
    else if (pLibInfo->flags.optimize)
    {
        if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) &&
            (pLibInfo->pPreRasterizationShaderLib != nullptr))
        {
            result = true;
        }
        else if ((pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT) &&
                 (pLibInfo->pFragmentOutputInterfaceLib != nullptr) &&
                 (enableRasterization == true))
        {
            result = true;
        }
    }

    return result;
}

// =====================================================================================================================
VkResult GraphicsPipelineCommon::Create(
    Device*                             pDevice,
    PipelineCache*                      pPipelineCache,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    PipelineCreateFlags                 flags,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipeline)
{
    VkResult result;

    const bool isLibrary = flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;

    if (isLibrary)
    {
        result =  GraphicsPipelineLibrary::Create(
            pDevice, pPipelineCache, pCreateInfo, flags, pAllocator, pPipeline);
    }
    else
    {
        result =  GraphicsPipeline::Create(
            pDevice, pPipelineCache, pCreateInfo, flags, pAllocator, pPipeline);
    }

    return result;
}

// =====================================================================================================================
static void CopyVertexInputInterfaceState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineObjectCreateInfo* pInfo)
{
    const GraphicsPipelineObjectCreateInfo& libInfo = pLibrary->GetPipelineObjectCreateInfo();

    pInfo->pipeline.iaState             = libInfo.pipeline.iaState;

    pInfo->immedInfo.inputAssemblyState = libInfo.immedInfo.inputAssemblyState;

    pInfo->staticStateMask             |= (libInfo.staticStateMask & ViiDynamicStatesMask);
}

// =====================================================================================================================
static void CopyPreRasterizationShaderState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineObjectCreateInfo* pInfo)
{
    const GraphicsPipelineObjectCreateInfo& libInfo = pLibrary->GetPipelineObjectCreateInfo();

    pInfo->immedInfo.inputAssemblyState.patchControlPoints = libInfo.immedInfo.inputAssemblyState.patchControlPoints;

    pInfo->pipeline.rsState            = libInfo.pipeline.rsState;
    pInfo->pipeline.viewportInfo       = libInfo.pipeline.viewportInfo;

    pInfo->immedInfo.msaaCreateInfo.conservativeRasterizationMode         =
        libInfo.immedInfo.msaaCreateInfo.conservativeRasterizationMode;
    pInfo->immedInfo.msaaCreateInfo.flags.enableConservativeRasterization =
        libInfo.immedInfo.msaaCreateInfo.flags.enableConservativeRasterization;
    pInfo->immedInfo.msaaCreateInfo.flags.enableLineStipple               =
        libInfo.immedInfo.msaaCreateInfo.flags.enableLineStipple;

    pInfo->immedInfo.triangleRasterState       = libInfo.immedInfo.triangleRasterState;
    pInfo->immedInfo.depthBiasParams           = libInfo.immedInfo.depthBiasParams;
    pInfo->immedInfo.pointLineRasterParams     = libInfo.immedInfo.pointLineRasterParams;
    pInfo->immedInfo.lineStippleParams         = libInfo.immedInfo.lineStippleParams;
    pInfo->immedInfo.graphicsShaderInfos.vs    = libInfo.immedInfo.graphicsShaderInfos.vs;
    pInfo->immedInfo.graphicsShaderInfos.hs    = libInfo.immedInfo.graphicsShaderInfos.hs;
    pInfo->immedInfo.graphicsShaderInfos.ds    = libInfo.immedInfo.graphicsShaderInfos.ds;
    pInfo->immedInfo.graphicsShaderInfos.gs    = libInfo.immedInfo.graphicsShaderInfos.gs;
    pInfo->immedInfo.graphicsShaderInfos.ts    = libInfo.immedInfo.graphicsShaderInfos.ts;
    pInfo->immedInfo.graphicsShaderInfos.ms    = libInfo.immedInfo.graphicsShaderInfos.ms;
    pInfo->immedInfo.graphicsShaderInfos.flags = libInfo.immedInfo.graphicsShaderInfos.flags;
    pInfo->immedInfo.viewportParams            = libInfo.immedInfo.viewportParams;
    pInfo->immedInfo.scissorRectParams         = libInfo.immedInfo.scissorRectParams;
    pInfo->immedInfo.rasterizerDiscardEnable   = libInfo.immedInfo.rasterizerDiscardEnable;

    pInfo->flags.bresenhamEnable = libInfo.flags.bresenhamEnable;
#if VKI_RAY_TRACING
    pInfo->flags.hasRayTracing  |= libInfo.flags.hasRayTracing;
#endif

    pInfo->staticStateMask      |= (libInfo.staticStateMask & PrsDynamicStatesMask);
}

// =====================================================================================================================
static void CopyFragmentShaderState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineObjectCreateInfo* pInfo)
{
    const GraphicsPipelineObjectCreateInfo& libInfo       = pLibrary->GetPipelineObjectCreateInfo();
    const GraphicsPipelineObjectImmedInfo& libImmedInfo   = libInfo.immedInfo;
    GraphicsPipelineObjectImmedInfo*        pDstImmedInfo = &pInfo->immedInfo;

    pDstImmedInfo->depthBoundParams                         = libImmedInfo.depthBoundParams;
    pDstImmedInfo->stencilRefMasks                          = libImmedInfo.stencilRefMasks;
    pDstImmedInfo->graphicsShaderInfos.ps                   = libImmedInfo.graphicsShaderInfos.ps;
    pDstImmedInfo->depthStencilCreateInfo.front             = libImmedInfo.depthStencilCreateInfo.front;
    pDstImmedInfo->depthStencilCreateInfo.back              = libImmedInfo.depthStencilCreateInfo.back;
    pDstImmedInfo->depthStencilCreateInfo.depthFunc         = libImmedInfo.depthStencilCreateInfo.depthFunc;
    pDstImmedInfo->depthStencilCreateInfo.depthEnable       = libImmedInfo.depthStencilCreateInfo.depthEnable;
    pDstImmedInfo->depthStencilCreateInfo.depthWriteEnable  = libImmedInfo.depthStencilCreateInfo.depthWriteEnable;
    pDstImmedInfo->depthStencilCreateInfo.depthBoundsEnable = libImmedInfo.depthStencilCreateInfo.depthBoundsEnable;
    pDstImmedInfo->depthStencilCreateInfo.stencilEnable     = libImmedInfo.depthStencilCreateInfo.stencilEnable;
    pDstImmedInfo->vrsRateParams                            = libImmedInfo.vrsRateParams;

#if VKI_RAY_TRACING
    pInfo->flags.hasRayTracing         |= libInfo.flags.hasRayTracing;
#endif
    pInfo->flags.fragmentShadingRateEnable |= libInfo.flags.fragmentShadingRateEnable;
    pInfo->staticStateMask            |= (libInfo.staticStateMask & FgsDynamicStatesMask);
}

// =====================================================================================================================
static void CopyFragmentOutputInterfaceState(
    const GraphicsPipelineLibrary*    pLibrary,
    GraphicsPipelineObjectCreateInfo* pInfo)
{
    const GraphicsPipelineObjectCreateInfo& libInfo = pLibrary->GetPipelineObjectCreateInfo();

    pInfo->pipeline.cbState.dualSourceBlendEnable     = libInfo.pipeline.cbState.dualSourceBlendEnable;
    pInfo->pipeline.cbState.logicOp                   = libInfo.pipeline.cbState.logicOp;
    pInfo->pipeline.cbState.uavExportSingleDraw       = libInfo.pipeline.cbState.uavExportSingleDraw;
    pInfo->pipeline.cbState.target[0].forceAlphaToOne = libInfo.pipeline.cbState.target[0].forceAlphaToOne;
    pInfo->pipeline.cbState.alphaToCoverageEnable     = libInfo.pipeline.cbState.alphaToCoverageEnable;
    for (uint32_t i = 0; i < MaxColorTargets; ++i)
    {
        pInfo->pipeline.cbState.target[i].swizzledFormat = libInfo.pipeline.cbState.target[i].swizzledFormat;
        pInfo->pipeline.cbState.target[i].channelWriteMask = libInfo.pipeline.cbState.target[i].channelWriteMask;
    }
    pInfo->pipeline.viewInstancingDesc = libInfo.pipeline.viewInstancingDesc;

    for (uint32_t i = 0; i < Pal::MaxColorTargets; ++i)
    {
        pInfo->immedInfo.blendCreateInfo.targets[i] = libInfo.immedInfo.blendCreateInfo.targets[i];
    }
    pInfo->immedInfo.blendConstParams                       = libInfo.immedInfo.blendConstParams;
    pInfo->immedInfo.logicOp                                = libInfo.immedInfo.logicOp;
    pInfo->immedInfo.logicOpEnable                          = libInfo.immedInfo.logicOpEnable;

    pInfo->immedInfo.colorWriteMask                         = libInfo.immedInfo.colorWriteMask;
    pInfo->immedInfo.colorWriteEnable                       = libInfo.immedInfo.colorWriteEnable;

    pInfo->immedInfo.msaaCreateInfo.coverageSamples         = libInfo.immedInfo.msaaCreateInfo.coverageSamples;
    pInfo->immedInfo.msaaCreateInfo.exposedSamples          = libInfo.immedInfo.msaaCreateInfo.exposedSamples;
    pInfo->immedInfo.msaaCreateInfo.pixelShaderSamples      = libInfo.immedInfo.msaaCreateInfo.pixelShaderSamples;
    pInfo->immedInfo.msaaCreateInfo.depthStencilSamples     = libInfo.immedInfo.msaaCreateInfo.depthStencilSamples;
    pInfo->immedInfo.msaaCreateInfo.shaderExportMaskSamples = libInfo.immedInfo.msaaCreateInfo.shaderExportMaskSamples;
    pInfo->immedInfo.msaaCreateInfo.sampleMask              = libInfo.immedInfo.msaaCreateInfo.sampleMask;
    pInfo->immedInfo.msaaCreateInfo.sampleClusters          = libInfo.immedInfo.msaaCreateInfo.sampleClusters;
    pInfo->immedInfo.msaaCreateInfo.alphaToCoverageSamples  = libInfo.immedInfo.msaaCreateInfo.alphaToCoverageSamples;
    pInfo->immedInfo.msaaCreateInfo.occlusionQuerySamples   = libInfo.immedInfo.msaaCreateInfo.occlusionQuerySamples;
    pInfo->immedInfo.msaaCreateInfo.flags.enable1xMsaaSampleLocations =
        libInfo.immedInfo.msaaCreateInfo.flags.enable1xMsaaSampleLocations;

    pInfo->immedInfo.samplePattern    = libInfo.immedInfo.samplePattern;
    pInfo->immedInfo.minSampleShading = libInfo.immedInfo.minSampleShading;

    pInfo->sampleCoverage               = libInfo.sampleCoverage;
    pInfo->flags.customMultiSampleState = libInfo.flags.customMultiSampleState;
    pInfo->flags.customSampleLocations  = libInfo.flags.customSampleLocations;
    pInfo->flags.force1x1ShaderRate     = libInfo.flags.force1x1ShaderRate;
    pInfo->flags.sampleShadingEnable    = libInfo.flags.sampleShadingEnable;

    pInfo->staticStateMask |= (libInfo.staticStateMask & FoiDynamicStatesMask);

    pInfo->dbFormat = libInfo.dbFormat;
}

// =====================================================================================================================
static void BuildRasterizationState(
    const Device*                                 pDevice,
    const VkPipelineRasterizationStateCreateInfo* pRs,
    const uint64_t                                dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*             pInfo)
{
    if (pRs != nullptr)
    {
        VK_ASSERT(pRs->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);

        // By default rasterization is disabled, unless rasterization creation info is present

        const RuntimeSettings&        settings         = pDevice->GetRuntimeSettings();
        const PhysicalDevice*         pPhysicalDevice  = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
        const VkPhysicalDeviceLimits& limits           = pPhysicalDevice->GetLimits();

        // Enable perpendicular end caps if we report strictLines semantics
        pInfo->pipeline.rsState.perpLineEndCapsEnable  = (limits.strictLines == VK_TRUE);

        pInfo->pipeline.rsState.dx10DiamondTestDisable = true;

        pInfo->pipeline.viewportInfo.depthClipNearEnable            = (pRs->depthClampEnable == VK_FALSE);
        pInfo->pipeline.viewportInfo.depthClipFarEnable             = (pRs->depthClampEnable == VK_FALSE);

        pInfo->immedInfo.triangleRasterState.frontFillMode          = VkToPalFillMode(pRs->polygonMode);
        pInfo->immedInfo.triangleRasterState.backFillMode           = VkToPalFillMode(pRs->polygonMode);
        pInfo->immedInfo.triangleRasterState.cullMode               = VkToPalCullMode(pRs->cullMode);
        pInfo->immedInfo.triangleRasterState.frontFace              = VkToPalFaceOrientation(pRs->frontFace);

        pInfo->immedInfo.triangleRasterState.flags.frontDepthBiasEnable = pRs->depthBiasEnable;
        pInfo->immedInfo.triangleRasterState.flags.backDepthBiasEnable  = pRs->depthBiasEnable;
        pInfo->immedInfo.depthBiasParams.depthBias                      = pRs->depthBiasConstantFactor;
        pInfo->immedInfo.depthBiasParams.depthBiasClamp                 = pRs->depthBiasClamp;
        pInfo->immedInfo.depthBiasParams.slopeScaledDepthBias           = pRs->depthBiasSlopeFactor;

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable) == true)
        {
            pInfo->immedInfo.rasterizerDiscardEnable = false;
        }
        else
        {
            pInfo->immedInfo.rasterizerDiscardEnable = pRs->rasterizerDiscardEnable;
        }

        if ((pRs->depthBiasEnable ||
             IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnable)) &&
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBias) == false))
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBias);
        }

        pInfo->immedInfo.pointLineRasterParams.lineWidth    = pRs->lineWidth;
        pInfo->immedInfo.pointLineRasterParams.pointSize    = DefaultPointSize;
        pInfo->immedInfo.pointLineRasterParams.pointSizeMin = limits.pointSizeRange[0];
        pInfo->immedInfo.pointLineRasterParams.pointSizeMax = limits.pointSizeRange[1];

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineWidth) == false)
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineWidth);
        }

        const void* pNext = pRs->pNext;

        while (pNext != nullptr)
        {
            const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<int32>(pHeader->sType))
            {
            // Handle extension specific structures
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
                {
                    const auto* pRsOrder = static_cast<const VkPipelineRasterizationStateRasterizationOrderAMD*>(pNext);

                    if (pPhysicalDevice->PalProperties().gfxipProperties.flags.supportOutOfOrderPrimitives)
                    {
                        pInfo->pipeline.rsState.outOfOrderPrimsEnable =
                            VkToPalRasterizationOrder(pRsOrder->rasterizationOrder);
                    }
                    break;
                }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsConservative =
                        static_cast<const VkPipelineRasterizationConservativeStateCreateInfoEXT*>(pNext);

                    // VK_EXT_conservative_rasterization must be enabled
                    VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_CONSERVATIVE_RASTERIZATION));
                    VK_ASSERT(pRsConservative->flags == 0);
                    VK_ASSERT(pRsConservative->conservativeRasterizationMode >=
                              VK_CONSERVATIVE_RASTERIZATION_MODE_BEGIN_RANGE_EXT);
                    VK_ASSERT(pRsConservative->conservativeRasterizationMode <=
                              VK_CONSERVATIVE_RASTERIZATION_MODE_END_RANGE_EXT);

                    switch (pRsConservative->conservativeRasterizationMode)
                    {
                    case VK_CONSERVATIVE_RASTERIZATION_MODE_DISABLED_EXT:
                        {
                            pInfo->immedInfo.msaaCreateInfo.flags.enableConservativeRasterization = false;
                        }
                        break;
                    case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
                        {
                            pInfo->immedInfo.msaaCreateInfo.flags.enableConservativeRasterization = true;
                            pInfo->immedInfo.msaaCreateInfo.conservativeRasterizationMode =
                                Pal::ConservativeRasterizationMode::Overestimate;
                        }
                        break;
                    case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
                        {
                            pInfo->immedInfo.msaaCreateInfo.flags.enableConservativeRasterization = true;
                            pInfo->immedInfo.msaaCreateInfo.conservativeRasterizationMode =
                                Pal::ConservativeRasterizationMode::Underestimate;
                        }
                        break;

                    default:
                        break;
                    }

                }
                break;
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsLine =
                        static_cast<const VkPipelineRasterizationLineStateCreateInfoEXT*>(pNext);

                    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineRasterizationMode) == false)
                    {
                        pInfo->flags.bresenhamEnable =
                            (pRsLine->lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT);

                        // Bresenham Lines need axis aligned end caps
                        if (pInfo->flags.bresenhamEnable)
                        {
                            pInfo->pipeline.rsState.perpLineEndCapsEnable = false;
                        }
                        else if (pRsLine->lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT)
                        {
                            pInfo->pipeline.rsState.perpLineEndCapsEnable = true;
                        }
                        pInfo->flags.perpLineEndCapsEnable = pInfo->pipeline.rsState.perpLineEndCapsEnable;
                    }

                    pInfo->immedInfo.msaaCreateInfo.flags.enableLineStipple                 = pRsLine->stippledLineEnable;

                    pInfo->immedInfo.lineStippleParams.lineStippleScale = (pRsLine->lineStippleFactor - 1);
                    pInfo->immedInfo.lineStippleParams.lineStippleValue = pRsLine->lineStipplePattern;

                    bool isDynamicLineStippleEnabled =
                        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineStippleEnable);
                    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineStipple) == false) &&
                        (pRsLine->stippledLineEnable || isDynamicLineStippleEnabled))
                    {
                        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStipple);
                    }
                }
                break;
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsDepthClip =
                        static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pNext);
                    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthClipEnable) == false)
                    {
                        pInfo->pipeline.viewportInfo.depthClipNearEnable = (pRsDepthClip->depthClipEnable == VK_TRUE);
                        pInfo->pipeline.viewportInfo.depthClipFarEnable = (pRsDepthClip->depthClipEnable == VK_TRUE);
                    }
                }
                break;
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsProvokingVertex =
                        static_cast<const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT*>(pNext);
                    pInfo->immedInfo.triangleRasterState.provokingVertex =
                        VkToPalProvokingVertex(pRsProvokingVertex->provokingVertexMode);
                }
                break;
            default:
                // Skip any unknown extension structures
                break;
            }

            pNext = pHeader->pNext;
        }

        if (pRs->depthClampEnable == VK_FALSE)
        {
            // For optimal performance, depth clamping should be enabled by default, even if API says otherwise.
            // Only disable it if dealing with depth values outside of [0.0, 1.0] range.
            // Otherwise clamp to [0.0, 1.0] interval.
            if(pDevice->IsExtensionEnabled(DeviceExtensions::EXT_DEPTH_RANGE_UNRESTRICTED) ||
              ((pInfo->pipeline.viewportInfo.depthClipNearEnable == false) &&
               (pInfo->pipeline.viewportInfo.depthClipFarEnable == false)))
            {
                pInfo->pipeline.rsState.depthClampMode = Pal::DepthClampMode::_None;
            }
            else
            {
                pInfo->pipeline.rsState.depthClampMode = Pal::DepthClampMode::ZeroToOne;
            }
        }
        else
        {
            // When depth clamping is enabled, depth clipping should be disabled, and vice versa.
            // Clipping is updated in pipeline compiler.
            pInfo->pipeline.rsState.depthClampMode = Pal::DepthClampMode::Viewport;
        }

        pInfo->pipeline.rsState.pointCoordOrigin       = Pal::PointOrigin::UpperLeft;
        pInfo->pipeline.rsState.shadeMode              = Pal::ShadeMode::Flat;
        pInfo->pipeline.rsState.rasterizeLastLinePixel = 0;

        // Pipeline Binning Override
        switch (pDevice->GetPipelineBinningMode())
        {
        case PipelineBinningModeEnable:
            pInfo->pipeline.rsState.binningOverride = Pal::BinningOverride::Enable;
            break;
        case PipelineBinningModeDisable:
            pInfo->pipeline.rsState.binningOverride = Pal::BinningOverride::Disable;
            break;
        case PipelineBinningModeDefault:
        default:
            pInfo->pipeline.rsState.binningOverride = Pal::BinningOverride::Default;
            break;
        }
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineRasterizationMode) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineRasterizationMode);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthClipEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthClampEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClampEnable);
    }
}

// =====================================================================================================================
static void BuildViewportState(
    const Device*                            pDevice,
    const VkPipelineViewportStateCreateInfo* pVp,
    const uint64_t                           dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*        pInfo)
{
    if (pVp != nullptr)
    {
        EXTRACT_VK_STRUCTURES_0(
            viewportDepthClipControl,
            PipelineViewportDepthClipControlCreateInfoEXT,
            static_cast<const VkPipelineViewportDepthClipControlCreateInfoEXT*>(pVp->pNext),
            PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT);

        // Default Vulkan depth range is [0, 1]
        // Check if VK_EXT_depth_clip_control overrides depth to [-1, 1]
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthClipNegativeOneToOne) == false)
        {
            pInfo->pipeline.viewportInfo.depthRange =
                ((pPipelineViewportDepthClipControlCreateInfoEXT != nullptr) &&
                    (pPipelineViewportDepthClipControlCreateInfoEXT->negativeOneToOne == VK_TRUE)) ?
                Pal::DepthRange::NegativeOneToOne : Pal::DepthRange::ZeroToOne;
        }
        else
        {
            pInfo->pipeline.viewportInfo.depthRange = Pal::DepthRange::ZeroToOne;
        }

        pInfo->immedInfo.viewportParams.depthRange = pInfo->pipeline.viewportInfo.depthRange;

        // From the spec, "scissorCount is the number of scissors and must match the number of viewports."
        VK_ASSERT(pVp->viewportCount <= Pal::MaxViewports);
        VK_ASSERT(pVp->scissorCount  <= Pal::MaxViewports);
        VK_ASSERT(pVp->scissorCount  == pVp->viewportCount);

        pInfo->immedInfo.viewportParams.count    = pVp->viewportCount;
        pInfo->immedInfo.scissorRectParams.count = pVp->scissorCount;

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::Viewport) == false)
        {
            VK_ASSERT(pVp->pViewports != nullptr);

            const bool maintenanceEnabled = pDevice->IsExtensionEnabled(DeviceExtensions::KHR_MAINTENANCE1);
            uint32_t   enabledApiVersion  = pDevice->VkPhysicalDevice(DefaultDeviceIndex)->GetEnabledAPIVersion();
            const bool khrMaintenance1    = (enabledApiVersion >= VK_MAKE_API_VERSION(0, 1, 1, 0)) || maintenanceEnabled;

            for (uint32_t i = 0; i < pVp->viewportCount; ++i)
            {
                VkToPalViewport(pVp->pViewports[i],
                        i,
                        khrMaintenance1,
                        &pInfo->immedInfo.viewportParams);
            }

            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::Viewport);
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::Scissor) == false)
        {
            VK_ASSERT(pVp->pScissors != nullptr);

            for (uint32_t i = 0; i < pVp->scissorCount; ++i)
            {
                VkToPalScissorRect(pVp->pScissors[i], i, &pInfo->immedInfo.scissorRectParams);
            }
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::Scissor);
        }
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthClipNegativeOneToOne) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthClipNegativeOneToOne);
    }
}

// =====================================================================================================================
static void BuildVrsRateParams(
    const Device*                                          pDevice,
    const VkPipelineFragmentShadingRateStateCreateInfoKHR* pFsr,
    const uint64_t                                         dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*                      pInfo)
{
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FragmentShadingRateStateKhr) == false)
    {
        if (pFsr != nullptr)
        {
            pInfo->immedInfo.vrsRateParams.flags.exposeVrsPixelsMask = 1;

            pInfo->immedInfo.vrsRateParams.shadingRate =
                VkToPalShadingSize(VkClampShadingRate(pFsr->fragmentSize, pDevice->GetMaxVrsShadingRate()));

            pInfo->immedInfo.vrsRateParams.combinerState[
                static_cast<uint32_t>(Pal::VrsCombinerStage::ProvokingVertex)] =
                VkToPalShadingRateCombinerOp(pFsr->combinerOps[0]);

            pInfo->immedInfo.vrsRateParams.combinerState[static_cast<uint32_t>(
                Pal::VrsCombinerStage::Primitive)] = VkToPalShadingRateCombinerOp(pFsr->combinerOps[0]);

            pInfo->immedInfo.vrsRateParams.combinerState[
                static_cast<uint32_t>(Pal::VrsCombinerStage::Image)] = VkToPalShadingRateCombinerOp(pFsr->combinerOps[1]);

            pInfo->immedInfo.vrsRateParams.combinerState[
                static_cast<uint32_t>(Pal::VrsCombinerStage::PsIterSamples)] = Pal::VrsCombiner::Passthrough;
            pInfo->flags.fragmentShadingRateEnable = 1;

        }
        pInfo->staticStateMask |=
            1ULL << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr);
    }
    else
    {
        pInfo->flags.fragmentShadingRateEnable = 1;
    }
}

// =====================================================================================================================
static void BuildMultisampleState(
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    const uint64_t                              dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*           pInfo)
{
    if (pMs != nullptr)
    {
        pInfo->flags.customMultiSampleState = true;
        pInfo->flags.force1x1ShaderRate =
            (pMs->sampleShadingEnable ||
            (pMs->rasterizationSamples == VK_SAMPLE_COUNT_8_BIT) ||
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples));

        pInfo->flags.sampleShadingEnable = pMs->sampleShadingEnable;

        // Sample Locations
        EXTRACT_VK_STRUCTURES_0(
            SampleLocations,
            PipelineSampleLocationsStateCreateInfoEXT,
            static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(pMs->pNext),
            PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT)

        if (pPipelineSampleLocationsStateCreateInfoEXT != nullptr)
        {
            pInfo->flags.customSampleLocations =
                IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocationsEnable) ||
                pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsEnable;
        }
        else
        {
            pInfo->flags.customSampleLocations =
                IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocationsEnable) &&
                IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocations);
        }

        uint32_t subpassCoverageSampleCount;
        uint32_t subpassColorSampleCount;
        uint32_t subpassDepthSampleCount;
        GraphicsPipelineCommon::GetSubpassSampleCount(
            pMs, pRenderPass, subpass,
            &subpassCoverageSampleCount, &subpassColorSampleCount, &subpassDepthSampleCount);

        pInfo->immedInfo.msaaCreateInfo.coverageSamples = subpassCoverageSampleCount;
        pInfo->immedInfo.msaaCreateInfo.exposedSamples  = subpassCoverageSampleCount;

        if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
        {
            pInfo->immedInfo.msaaCreateInfo.pixelShaderSamples =
                Pow2Pad(static_cast<uint32_t>(ceil(subpassColorSampleCount * pMs->minSampleShading)));
            pInfo->immedInfo.minSampleShading = pMs->minSampleShading;
        }
        else
        {
            pInfo->immedInfo.msaaCreateInfo.pixelShaderSamples = 1;
            pInfo->immedInfo.minSampleShading = 0.0f;
        }

        pInfo->immedInfo.msaaCreateInfo.depthStencilSamples = subpassDepthSampleCount;
        pInfo->immedInfo.msaaCreateInfo.shaderExportMaskSamples = subpassCoverageSampleCount;
        pInfo->immedInfo.msaaCreateInfo.sampleMask = (pMs->pSampleMask != nullptr)
                                    ? pMs->pSampleMask[0]
                                    : 0xffffffff;
        pInfo->immedInfo.msaaCreateInfo.sampleClusters         = subpassCoverageSampleCount;
        pInfo->immedInfo.msaaCreateInfo.alphaToCoverageSamples = subpassCoverageSampleCount;
        pInfo->immedInfo.msaaCreateInfo.occlusionQuerySamples  = subpassDepthSampleCount;
        pInfo->sampleCoverage              = subpassCoverageSampleCount;

        pInfo->pipeline.cbState.target[0].forceAlphaToOne = (pMs->alphaToOneEnable == VK_TRUE);
        pInfo->pipeline.cbState.alphaToCoverageEnable = (pMs->alphaToCoverageEnable == VK_TRUE) ||
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::AlphaToCoverageEnable) == true);

        if (pInfo->flags.customSampleLocations)
        {
            // Enable single-sampled custom sample locations if necessary
            pInfo->immedInfo.msaaCreateInfo.flags.enable1xMsaaSampleLocations =
                (pInfo->immedInfo.msaaCreateInfo.coverageSamples == 1);

            if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocations) == false) &&
                (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples) == false))
            {
                if (pPipelineSampleLocationsStateCreateInfoEXT != nullptr)
                {
                    // We store the custom sample locations if custom sample locations are enabled and the
                    // sample locations state is static and rasterizationSamples is not configured dynamically.
                    pInfo->immedInfo.samplePattern.sampleCount =
                        (uint32_t)pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationsPerPixel;

                    ConvertToPalMsaaQuadSamplePattern(
                        &pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo,
                        &pInfo->immedInfo.samplePattern.locations);

                    VK_ASSERT(pInfo->immedInfo.samplePattern.sampleCount ==
                        static_cast<uint32_t>(pMs->rasterizationSamples));

                }
                else
                {
                    pInfo->immedInfo.samplePattern.sampleCount = pMs->rasterizationSamples;
                    pInfo->immedInfo.samplePattern.locations =
                        *Device::GetDefaultQuadSamplePattern(pMs->rasterizationSamples);
                }
                pInfo->staticStateMask |=
                    (1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocations));
            }
        }
        else if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples) == false)
        {
            // We store the standard sample locations if custom sample locations are not enabled and
            // rasterizationSamples is not configured dynamically.
            pInfo->immedInfo.samplePattern.sampleCount = pMs->rasterizationSamples;
            pInfo->immedInfo.samplePattern.locations =
                *Device::GetDefaultQuadSamplePattern(pMs->rasterizationSamples);
        }
    }
    else
    {
        pInfo->pipeline.cbState.alphaToCoverageEnable =
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::AlphaToCoverageEnable) == true);
        pInfo->flags.customSampleLocations =
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocationsEnable) &&
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocations);
    }
}

// =====================================================================================================================
static void BuildDepthStencilState(
    const VkPipelineDepthStencilStateCreateInfo* pDs,
    const uint64_t                               dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*            pInfo)
{
    if (pDs != nullptr)
    {
        pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = (pDs->stencilTestEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = (pDs->depthTestEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = (pDs->depthWriteEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = VkToPalCompareFunc(pDs->depthCompareOp);
        pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = (pDs->depthBoundsTestEnable == VK_TRUE);

        pInfo->immedInfo.depthStencilCreateInfo.front.stencilFailOp      = VkToPalStencilOp(pDs->front.failOp);
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilPassOp      = VkToPalStencilOp(pDs->front.passOp);
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilDepthFailOp = VkToPalStencilOp(pDs->front.depthFailOp);
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilFunc        = VkToPalCompareFunc(pDs->front.compareOp);
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilFailOp       = VkToPalStencilOp(pDs->back.failOp);
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilPassOp       = VkToPalStencilOp(pDs->back.passOp);
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilDepthFailOp  = VkToPalStencilOp(pDs->back.depthFailOp);
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilFunc         = VkToPalCompareFunc(pDs->back.compareOp);

        pInfo->immedInfo.stencilRefMasks.frontRef       = static_cast<uint8_t>(pDs->front.reference);
        pInfo->immedInfo.stencilRefMasks.frontReadMask  = static_cast<uint8_t>(pDs->front.compareMask);
        pInfo->immedInfo.stencilRefMasks.frontWriteMask = static_cast<uint8_t>(pDs->front.writeMask);
        pInfo->immedInfo.stencilRefMasks.backRef        = static_cast<uint8_t>(pDs->back.reference);
        pInfo->immedInfo.stencilRefMasks.backReadMask   = static_cast<uint8_t>(pDs->back.compareMask);
        pInfo->immedInfo.stencilRefMasks.backWriteMask  = static_cast<uint8_t>(pDs->back.writeMask);

        pInfo->immedInfo.depthBoundParams.min = pDs->minDepthBounds;
        pInfo->immedInfo.depthBoundParams.max = pDs->maxDepthBounds;
    }

    pInfo->immedInfo.stencilRefMasks.frontOpValue = DefaultStencilOpValue;
    pInfo->immedInfo.stencilRefMasks.backOpValue  = DefaultStencilOpValue;

    if ((pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnable)) &&
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBounds) == false))
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilCompareMask) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilWriteMask) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilReference) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilReference);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthWriteEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthTestEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthCompareOp) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOp);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilTestEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilOp) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilOp);
    }
}

// =====================================================================================================================
static void BuildColorBlendState(
    const Device*                              pDevice,
    const VkPipelineRenderingCreateInfoKHR*    pRendering,
    const VkPipelineColorBlendStateCreateInfo* pCb,
    const RenderPass*                          pRenderPass,
    const uint32_t                             subpass,
    const uint64_t                             dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*          pInfo)
{
    bool blendingEnabled = false;
    bool dualSourceBlend = false;

    pInfo->pipeline.cbState.logicOp = Pal::LogicOp::Copy;
    pInfo->immedInfo.logicOp = VK_LOGIC_OP_MAX_ENUM;
    pInfo->immedInfo.logicOpEnable = false;

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LogicOp) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOp);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LogicOpEnable) == false)
    {
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LogicOp) == false)
        {
            pInfo->pipeline.cbState.logicOp =
                ((pCb != nullptr) && pCb->logicOpEnable) ? VkToPalLogicOp(pCb->logicOp) : Pal::LogicOp::Copy;
        }

        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LogicOpEnable);
    }

    if (GetColorAttachmentCount(pRenderPass, subpass, pRendering) != 0)
    {
        if (pCb != nullptr)
        {
            pInfo->immedInfo.logicOp = pCb->logicOp;
            pInfo->immedInfo.logicOpEnable = pCb->logicOpEnable;
        }

        uint32_t numColorTargets = 0;
        const VkPipelineColorWriteCreateInfoEXT* pColorWriteCreateInfo = nullptr;
        if (pCb != nullptr)
        {
            numColorTargets = Min(pCb->attachmentCount, Pal::MaxColorTargets);

            const void* pNext = static_cast<const VkStructHeader*>(pCb->pNext);

            while (pNext != nullptr)
            {
                const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

                switch (static_cast<uint32>(pHeader->sType))
                {
                    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
                    {
                        break;
                    }
                    case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
                    {
                        pColorWriteCreateInfo = reinterpret_cast<const VkPipelineColorWriteCreateInfoEXT*>(pHeader);
                        break;
                    }

                    default:
                        // Skip any unknown extension structures
                        break;
                }

                pNext = pHeader->pNext;
            }
        }

        if (pRendering != nullptr)
        {
            numColorTargets = Min(pRendering->colorAttachmentCount, Pal::MaxColorTargets);
        }

        pInfo->immedInfo.colorWriteEnable = 0;
        pInfo->immedInfo.colorWriteMask = 0;

        if (numColorTargets > 0)
        {
            for (uint32_t i = 0; i < numColorTargets; ++i)
            {
                auto pCbDst     = &pInfo->pipeline.cbState.target[i];

                if (pRenderPass != nullptr)
                {
                    const VkFormat cbFormat = pRenderPass->GetColorAttachmentFormat(subpass, i);
                    pCbDst->swizzledFormat  = VkToPalFormat(cbFormat, pDevice->GetRuntimeSettings());
                }
                else if (pRendering != nullptr)
                {
                    const VkFormat cbFormat = pRendering->pColorAttachmentFormats[i];
                    pCbDst->swizzledFormat  = VkToPalFormat(cbFormat, pDevice->GetRuntimeSettings());
                }
                // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
                // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
                // disable shader writes through that target.
                if (pCbDst->swizzledFormat.format != Pal::ChNumFormat::Undefined)
                {
                    const VkPipelineColorBlendAttachmentState* pSrc =
                        (pCb != nullptr) ? &pCb->pAttachments[i] : nullptr;
                    VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                           VK_COLOR_COMPONENT_G_BIT |
                                                           VK_COLOR_COMPONENT_B_BIT |
                                                           VK_COLOR_COMPONENT_A_BIT;
                    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteMask) == false)
                    {
                        if (pSrc != nullptr)
                        {
                            colorWriteMask = pSrc->colorWriteMask;
                        }
                    }

                    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteEnable) == false) &&
                        (pColorWriteCreateInfo != nullptr) &&
                        (pColorWriteCreateInfo->pColorWriteEnables != nullptr) &&
                        (pColorWriteCreateInfo->pColorWriteEnables[i] == false))
                    {
                        pCbDst->channelWriteMask = 0;
                    }
                    else
                    {
                        pCbDst->channelWriteMask           = colorWriteMask;
                        pInfo->immedInfo.colorWriteMask   |= colorWriteMask << (4 * i);
                        pInfo->immedInfo.colorWriteEnable |= (0xF << (4 * i));
                    }

                    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable) == false))
                    {
                        if (pSrc != nullptr)
                        {
                            blendingEnabled |= (pSrc->blendEnable == VK_TRUE);
                        }
                    }
                }
            }

            if ((pCb != nullptr) &&
                ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable) == false) ||
                 (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation) == false)))
            {
                BuildPalColorBlendStateCreateInfo(pCb, dynamicStateFlags, &pInfo->immedInfo.blendCreateInfo);
                dualSourceBlend = GraphicsPipelineCommon::GetDualSourceBlendEnableState(
                    pDevice, pCb, &pInfo->immedInfo.blendCreateInfo);
            }
        }

        pInfo->pipeline.cbState.dualSourceBlendEnable = dualSourceBlend;
    }

    if (((blendingEnabled == true) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable)) &&
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::BlendConstants) == false))
    {
        static_assert(sizeof(pInfo->immedInfo.blendConstParams) == sizeof(pCb->blendConstants),
            "Blend constant structure size mismatch!");
        if (pCb != nullptr)
        {
            memcpy(&pInfo->immedInfo.blendConstParams, pCb->blendConstants, sizeof(pCb->blendConstants));
        }
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants);
    }
}

// =====================================================================================================================
static void BuildRenderingState(
    const Device*                                 pDevice,
    const VkPipelineRenderingCreateInfoKHR*       pRendering,
    const VkPipelineColorBlendStateCreateInfo*    pCb,
    const RenderPass*                             pRenderPass,
    GraphicsPipelineObjectCreateInfo*             pInfo)
{
    pInfo->pipeline.viewInstancingDesc = {};

    if (((pRenderPass != nullptr) && pRenderPass->IsMultiviewEnabled()) ||
        ((pRendering != nullptr) && (Util::CountSetBits(pRendering->viewMask)!= 0)))
    {
        pInfo->pipeline.viewInstancingDesc.viewInstanceCount = Pal::MaxViewInstanceCount;
        pInfo->pipeline.viewInstancingDesc.enableMasking     = true;

        for (uint32 viewIndex = 0; viewIndex < Pal::MaxViewInstanceCount; ++viewIndex)
        {
            pInfo->pipeline.viewInstancingDesc.viewId[viewIndex] = viewIndex;
        }
    }
}

// =====================================================================================================================
static void BuildVertexInputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const VbBindingInfo*                pVbInfo,
    const uint64_t                      dynamicStateFlags,
    const bool                          isLibrary,
    const bool                          hasMesh,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    const VkPipelineInputAssemblyStateCreateInfo* pIa = pIn->pInputAssemblyState;

    pInfo->immedInfo.inputAssemblyState.primitiveRestartIndex = 0xFFFFFFFF;
    if ((pIa != nullptr) &&
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopology) == false))
    {
        pInfo->immedInfo.inputAssemblyState.primitiveRestartEnable = (pIa->primitiveRestartEnable != VK_FALSE);
    }
    else
    {
        pInfo->immedInfo.inputAssemblyState.primitiveRestartEnable = false;
    }

    pInfo->immedInfo.inputAssemblyState.topology = Pal::PrimitiveTopology::TriangleList;
    pInfo->pipeline.iaState.topologyInfo.primitiveType = Pal::PrimitiveType::Triangle;
    if (pIa != nullptr)
    {
        pInfo->immedInfo.inputAssemblyState.topology       = VkToPalPrimitiveTopology(pIa->topology);
        pInfo->pipeline.iaState.topologyInfo.primitiveType = VkToPalPrimitiveType(pIa->topology);
    }

    if ((pIn->pVertexInputState != nullptr) ||
        isLibrary ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInput))
    {
        pInfo->pipeline.iaState.vertexBufferCount = pVbInfo->bindingTableSize;
    }

    if (hasMesh == false)
    {
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopology) == false)
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopology);
        }
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInputBindingStride) == false)
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStride);
        }
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInput) == false)
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::VertexInput);
        }
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveRestartEnable) == false)
        {
            pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnable);
        }
    }
}

// =====================================================================================================================
static void BuildPreRasterizationShaderState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint64_t                      dynamicStateFlags,
#if VKI_RAY_TRACING
    const bool                          hasRayTracing,
#endif
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    if (pIn->pTessellationState != nullptr)
    {
        pInfo->immedInfo.inputAssemblyState.patchControlPoints = pIn->pTessellationState->patchControlPoints;
    }
    else
    {
        pInfo->immedInfo.inputAssemblyState.patchControlPoints = 0;
    }

    // Build states via VkPipelineRasterizationStateCreateInfo
    BuildRasterizationState(pDevice, pIn->pRasterizationState, dynamicStateFlags, pInfo);

    if (pInfo->immedInfo.rasterizerDiscardEnable == false)
    {
        // Build states via VkPipelineViewportStateCreateInfo
        BuildViewportState(pDevice, pIn->pViewportState, dynamicStateFlags, pInfo);
    }

#if VKI_RAY_TRACING
    pInfo->flags.hasRayTracing |= hasRayTracing;
#endif

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::CullMode) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::CullMode);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FrontFace) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::FrontFace);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PolygonMode) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::PolygonMode);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ProvokingVertexMode) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ProvokingVertexMode);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ViewportCount) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ScissorCount) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ConservativeRasterizationMode) == false)
    {
        pInfo->staticStateMask |=
            1ULL << static_cast<uint32_t>(DynamicStatesInternal::ConservativeRasterizationMode);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ExtraPrimitiveOverestimationSize) == false)
    {
        pInfo->staticStateMask |=
            1ULL << static_cast<uint32_t>(DynamicStatesInternal::ExtraPrimitiveOverestimationSize);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineStippleEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::LineStippleEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::TessellationDomainOrigin) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::TessellationDomainOrigin);
    }
}

// =====================================================================================================================
static void BuildFragmentShaderState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint64_t                      dynamicStateFlags,
#if VKI_RAY_TRACING
    const bool                          hasRayTracing,
#endif
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    // Build states via VkPipelineDepthStencilStateCreateInfo
    BuildDepthStencilState(pIn->pDepthStencilState, dynamicStateFlags, pInfo);

    // Build VRS state
    EXTRACT_VK_STRUCTURES_0(
        variableRateShading,
        PipelineFragmentShadingRateStateCreateInfoKHR,
        static_cast<const VkPipelineFragmentShadingRateStateCreateInfoKHR*>(pIn->pNext),
        PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR)

    BuildVrsRateParams(pDevice, pPipelineFragmentShadingRateStateCreateInfoKHR, dynamicStateFlags, pInfo);

#if VKI_RAY_TRACING
    pInfo->flags.hasRayTracing |= hasRayTracing;
#endif
}

// =====================================================================================================================
static void BuildFragmentOutputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint64_t                      dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    const RenderPass* pRenderPass      = RenderPass::ObjectFromHandle(pIn->renderPass);
    const uint32_t    subpass          = pIn->subpass;

    // Build states via VkPipelineMultisampleStateCreateInfo
    BuildMultisampleState(pIn->pMultisampleState, pRenderPass, subpass, dynamicStateFlags, pInfo);

    // Extract VkPipelineRenderingFormatCreateInfoKHR for VK_KHR_dynamic_rendering extension
    EXTRACT_VK_STRUCTURES_0(
        renderingCreateInfo,
        PipelineRenderingCreateInfoKHR,
        static_cast<const VkPipelineRenderingCreateInfoKHR*>(pIn->pNext),
        PIPELINE_RENDERING_CREATE_INFO_KHR);

    pInfo->dbFormat = GetDepthFormat(pRenderPass, subpass, pPipelineRenderingCreateInfoKHR);

    // Build states via VkPipelineColorBlendStateCreateInfo
    BuildColorBlendState(
        pDevice,
        pPipelineRenderingCreateInfoKHR,
        pIn->pColorBlendState,
        pRenderPass,
        subpass,
        dynamicStateFlags,
        pInfo);

    BuildRenderingState(pDevice,
                        pPipelineRenderingCreateInfoKHR,
                        pIn->pColorBlendState,
                        pRenderPass,
                        pInfo);

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteMask) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteMask);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::ColorBlendEquation);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::RasterizationSamples);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleMask) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleMask);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::AlphaToCoverageEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::AlphaToCoverageEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocationsEnable) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsEnable);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocations) == false)
    {
        pInfo->staticStateMask |= 1ULL << static_cast<uint32_t>(DynamicStatesInternal::SampleLocations);
    }
}

// =====================================================================================================================
static void BuildExecutablePipelineState(
    const bool                          hasMesh,
    const uint64_t                      dynamicStateFlags,
    const PipelineMetadata*             pBinMeta,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    if (pInfo->immedInfo.rasterizerDiscardEnable == true)
    {
        pInfo->pipeline.viewportInfo.depthRange = Pal::DepthRange::ZeroToOne;
        pInfo->pipeline.cbState.logicOp         = Pal::LogicOp::Copy;

        pInfo->immedInfo.depthStencilCreateInfo.front.stencilFailOp      = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilPassOp      = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilDepthFailOp = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.front.stencilFunc        = Pal::CompareFunc::Never;
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilFailOp       = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilPassOp       = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilDepthFailOp  = Pal::StencilOp::Keep;
        pInfo->immedInfo.depthStencilCreateInfo.back.stencilFunc         = Pal::CompareFunc::Never;

        pInfo->immedInfo.stencilRefMasks.frontRef       = 0;
        pInfo->immedInfo.stencilRefMasks.frontReadMask  = 0;
        pInfo->immedInfo.stencilRefMasks.frontWriteMask = 0;
        pInfo->immedInfo.stencilRefMasks.backRef        = 0;
        pInfo->immedInfo.stencilRefMasks.backReadMask   = 0;
        pInfo->immedInfo.stencilRefMasks.backWriteMask  = 0;

        pInfo->immedInfo.depthBoundParams.min = 0.0f;
        pInfo->immedInfo.depthBoundParams.max = 0.0f;

        pInfo->flags.force1x1ShaderRate = false;

        memset(&(pInfo->immedInfo.vrsRateParams),              0, sizeof(pInfo->immedInfo.vrsRateParams));
        memset(&(pInfo->immedInfo.viewportParams),             0, sizeof(pInfo->immedInfo.viewportParams));
        memset(&(pInfo->immedInfo.scissorRectParams),          0, sizeof(pInfo->immedInfo.scissorRectParams));
        memset(&(pInfo->pipeline.cbState.target[0]),           0, sizeof(pInfo->pipeline.cbState.target));
        memset(&(pInfo->immedInfo.blendCreateInfo.targets[0]), 0, sizeof(pInfo->immedInfo.blendCreateInfo.targets));

        pInfo->staticStateMask &=
            ~((1ULL << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr)) |
              (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Viewport)) |
              (1ULL << static_cast<uint32_t>(DynamicStatesInternal::Scissor)));
    }

    if (pInfo->dbFormat == VK_FORMAT_UNDEFINED)
    {
        pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = false;
        pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = false;
        pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = Pal::CompareFunc::Always;
        pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = false;
        pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = false;

        pInfo->staticStateMask &=
            ~((1ULL << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds)) |
              (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask)) |
              (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask)) |
              (1ULL << static_cast<uint32_t>(DynamicStatesInternal::StencilReference)));
    }

    if (pInfo->flags.force1x1ShaderRate == true)
    {
        pInfo->immedInfo.vrsRateParams.shadingRate = Pal::VrsShadingRate::_1x1;

        for (uint32 idx = 0; idx <= static_cast<uint32>(Pal::VrsCombinerStage::Image); idx++)
        {
            pInfo->immedInfo.vrsRateParams.combinerState[idx] = Pal::VrsCombiner::Passthrough;
        }
    }

    if ((pInfo->immedInfo.rasterizerDiscardEnable == true) ||
        (pInfo->flags.customMultiSampleState == false) ||
        ((pInfo->flags.bresenhamEnable == true) && (pInfo->flags.customSampleLocations == false)))
    {
        pInfo->immedInfo.msaaCreateInfo.coverageSamples         = 1;
        pInfo->immedInfo.msaaCreateInfo.exposedSamples          = 0;
        pInfo->immedInfo.msaaCreateInfo.pixelShaderSamples      = 1;
        pInfo->immedInfo.msaaCreateInfo.depthStencilSamples     = 1;
        pInfo->immedInfo.msaaCreateInfo.shaderExportMaskSamples = 1;
        pInfo->immedInfo.msaaCreateInfo.sampleMask              = 1;
        pInfo->immedInfo.msaaCreateInfo.sampleClusters          = 1;
        pInfo->immedInfo.msaaCreateInfo.alphaToCoverageSamples  = 1;
        pInfo->immedInfo.msaaCreateInfo.occlusionQuerySamples   = 1;

        pInfo->sampleCoverage = 1;

        pInfo->immedInfo.samplePattern.sampleCount = 1;
        pInfo->immedInfo.samplePattern.locations   = *Device::GetDefaultQuadSamplePattern(1);

        pInfo->flags.sampleShadingEnable = false;
    }

#if PAL_BUILD_GFX103
    // Both MSAA and VRS would utilize the value of PS_ITER_SAMPLES
    // Thus, choose the min combiner (i.e. choose the higher quality rate) when both features are enabled
    if ((pInfo->immedInfo.msaaCreateInfo.pixelShaderSamples > 1) &&
        (pInfo->immedInfo.vrsRateParams.flags.exposeVrsPixelsMask == 1))
    {
        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::PsIterSamples)] = Pal::VrsCombiner::Min;
    }
#endif

    pInfo->flags.bindDepthStencilObject =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilOp) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilTestEnable) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnable) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthCompareOp) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthWriteEnable) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthTestEnable));

    pInfo->flags.bindTriangleRasterState =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::CullMode) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FrontFace) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnable) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PolygonMode) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ProvokingVertexMode));

    pInfo->flags.bindStencilRefMasks =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilCompareMask) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilWriteMask) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilReference));

    pInfo->flags.bindInputAssemblyState =
        (hasMesh == false) &&
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopology) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveRestartEnable));

    pInfo->flags.bindColorBlendObject =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEnable) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorBlendEquation));

    pInfo->flags.bindMsaaObject =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizationSamples) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleMask) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ConservativeRasterizationMode) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineStippleEnable));
}

// =====================================================================================================================
void GraphicsPipelineCommon::BuildPipelineObjectCreateInfo(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pIn,
    PipelineCreateFlags                    flags,
    const GraphicsPipelineShaderStageInfo* pShaderStageInfo,
    const PipelineLayout*                  pPipelineLayout,
    const PipelineOptimizerKey*            pOptimizerKey,
    const PipelineMetadata*                pBinMeta,
    GraphicsPipelineObjectCreateInfo*      pInfo)
{
    VK_ASSERT(pBinMeta != nullptr);

    GraphicsPipelineLibraryInfo libInfo;
    ExtractLibraryInfo(pIn, flags, &libInfo);

    bool hasMesh = false;
#if VKI_RAY_TRACING
    bool hasRayTracing = false;
#endif

    for (uint32_t stageIdx = 0; stageIdx < ShaderStage::ShaderStageGfxCount; ++stageIdx)
    {
        if (pShaderStageInfo->stages[stageIdx].pModuleHandle != nullptr)
        {
            const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
                ShaderModule::GetFirstValidShaderData(pShaderStageInfo->stages[stageIdx].pModuleHandle));

            VK_ASSERT(pModuleData != nullptr);

#if VKI_RAY_TRACING
            if (pModuleData->usage.enableRayQuery != 0)
            {
                hasRayTracing = true;
            }
#endif
            if (stageIdx == ShaderStageMesh)
            {
                hasMesh = true;
            }
        }
    }

    if (libInfo.pPreRasterizationShaderLib != nullptr)
    {
        if (Util::TestAnyFlagSet(libInfo.pPreRasterizationShaderLib->GetPipelineObjectCreateInfo().activeStages,
                                 VK_SHADER_STAGE_MESH_BIT_EXT))
        {
            hasMesh = true;
        }
    }

    uint32_t libFlags = libInfo.libFlags;

    pInfo->activeStages = GetActiveShaderStages(pIn, &libInfo);

    uint64_t dynamicStateFlags = GetDynamicStateFlags(pIn->pDynamicState, &libInfo);

    libInfo.libFlags = libFlags;
    pInfo->dynamicStates = dynamicStateFlags;

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
    {
        BuildVertexInputInterfaceState(
            pDevice, pIn, &pBinMeta->vbInfo, dynamicStateFlags, libInfo.flags.isLibrary, hasMesh, pInfo);
    }
    else if (libInfo.pVertexInputInterfaceLib != nullptr)
    {
        CopyVertexInputInterfaceState(libInfo.pVertexInputInterfaceLib, pInfo);
    }

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        BuildPreRasterizationShaderState(pDevice,
                                         pIn,
                                         dynamicStateFlags,
#if VKI_RAY_TRACING
                                         hasRayTracing,
#endif
                                         pInfo);
    }
    else if (libInfo.pPreRasterizationShaderLib != nullptr)
    {
        CopyPreRasterizationShaderState(libInfo.pPreRasterizationShaderLib, pInfo);
    }

    const bool enableRasterization =
        (~libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
        (pInfo->immedInfo.rasterizerDiscardEnable == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable);

    if (enableRasterization)
    {
        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
        {
            BuildFragmentShaderState(pDevice,
                                     pIn,
                                     dynamicStateFlags,
#if VKI_RAY_TRACING
                                     hasRayTracing,
#endif
                                     pInfo);
        }
        else if (libInfo.pFragmentShaderLib != nullptr)
        {
            CopyFragmentShaderState(libInfo.pFragmentShaderLib, pInfo);

        }

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
        {
            BuildFragmentOutputInterfaceState(pDevice, pIn, dynamicStateFlags, pInfo);
        }
        else if (libInfo.pFragmentOutputInterfaceLib != nullptr)
        {
            CopyFragmentOutputInterfaceState(libInfo.pFragmentOutputInterfaceLib, pInfo);
        }
    }

    if (libInfo.flags.isLibrary == false)
    {
        BuildExecutablePipelineState(hasMesh, dynamicStateFlags, pBinMeta, pInfo);

        if (pOptimizerKey != nullptr)
        {
            pDevice->GetShaderOptimizer()->OverrideGraphicsPipelineCreateInfo(
                *pOptimizerKey,
                pInfo->activeStages,
                &pInfo->pipeline,
                &pInfo->immedInfo.graphicsShaderInfos);
        }
    }
}

// =====================================================================================================================
// Populates the profile key for tuning graphics pipelines
void GraphicsPipelineCommon::GeneratePipelineOptimizerKey(
    const Device*                          pDevice,
    const VkGraphicsPipelineCreateInfo*    pCreateInfo,
    PipelineCreateFlags                    flags,
    const GraphicsPipelineShaderStageInfo* pShaderStageInfo,
    ShaderOptimizerKey*                    pShaderKeys,
    PipelineOptimizerKey*                  pPipelineKey)
{
    GraphicsPipelineLibraryInfo libInfo;
    GraphicsPipelineCommon::ExtractLibraryInfo(pCreateInfo, flags, &libInfo);

    pPipelineKey->shaderCount = VK_ARRAY_SIZE(pShaderStageInfo->stages);
    pPipelineKey->pShaders    = pShaderKeys;

    for (uint32_t stageIdx = 0; stageIdx < pPipelineKey->shaderCount; ++stageIdx)
    {
        uint32_t stageBit = 1 << stageIdx;
        if ((PrsShaderMask & stageBit) != 0)
        {
            // Pre-resterization Stages
            if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
            {
                const auto& stage = pShaderStageInfo->stages[stageIdx];
                if (stage.pModuleHandle != nullptr)
                {
                    const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
                        ShaderModule::GetFirstValidShaderData(stage.pModuleHandle));

                    VK_ASSERT(pModuleData != nullptr);

                    pDevice->GetShaderOptimizer()->CreateShaderOptimizerKey(
                        pModuleData,
                        stage.codeHash,
                        static_cast<Vkgc::ShaderStage>(stageIdx),
                        stage.codeSize,
                        &pShaderKeys[stageIdx]);
                }
            }
            else if (libInfo.pPreRasterizationShaderLib != nullptr)
            {
                const auto& libBinaryInfo = libInfo.pPreRasterizationShaderLib->GetPipelineBinaryCreateInfo();
                pShaderKeys[stageIdx]     = libBinaryInfo.pPipelineProfileKey->pShaders[stageIdx];
            }
        }
        else if ((FgsShaderMask & stageBit) != 0)
        {
            // Framgment stage
            if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
            {
                const auto& stage = pShaderStageInfo->stages[stageIdx];
                if (stage.pModuleHandle != nullptr)
                {
                    const auto* pModuleData = reinterpret_cast<const Vkgc::ShaderModuleData*>(
                        ShaderModule::GetFirstValidShaderData(stage.pModuleHandle));

                    VK_ASSERT(pModuleData != nullptr);

                    pDevice->GetShaderOptimizer()->CreateShaderOptimizerKey(
                        pModuleData,
                        stage.codeHash,
                        static_cast<Vkgc::ShaderStage>(stageIdx),
                        stage.codeSize,
                        &pShaderKeys[stageIdx]);
                }
            }
            else if (libInfo.pFragmentShaderLib != nullptr)
            {
                const auto& libBinaryInfo = libInfo.pFragmentShaderLib->GetPipelineBinaryCreateInfo();
                pShaderKeys[stageIdx]     = libBinaryInfo.pPipelineProfileKey->pShaders[stageIdx];
            }
        }
        else
        {
            VK_NEVER_CALLED();
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineVertexInputStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.pVertexBindingDescriptions
//     - desc.pVertexAttributeDescriptions
//     - pDivisorStateCreateInfo->pVertexBindingDivisors
static void GenerateHashFromVertexInputStateCreateInfo(
    const VkPipelineVertexInputStateCreateInfo& desc,
    Util::MetroHash128*                         pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.vertexBindingDescriptionCount);

    for (uint32_t i = 0; i < desc.vertexBindingDescriptionCount; i++)
    {
        pHasher->Update(desc.pVertexBindingDescriptions[i]);
    }

    pHasher->Update(desc.vertexAttributeDescriptionCount);

    for (uint32_t i = 0; i < desc.vertexAttributeDescriptionCount; i++)
    {
        pHasher->Update(desc.pVertexAttributeDescriptions[i]);
    }

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_DIVISOR_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineVertexInputDivisorStateCreateInfoEXT*>(pNext);
                pHasher->Update(pExtInfo->sType);
                pHasher->Update(pExtInfo->vertexBindingDivisorCount);

                for (uint32 i = 0; i < pExtInfo->vertexBindingDivisorCount; i++)
                {
                    pHasher->Update(pExtInfo->pVertexBindingDivisors[i]);
                }

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineInputAssemblyStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.topology
static void GenerateHashFromInputAssemblyStateCreateInfo(
    const VkPipelineInputAssemblyStateCreateInfo& desc,
    Util::MetroHash128*                           pElfHasher,
    Util::MetroHash128*                           pApiHasher)
{
    pElfHasher->Update(desc.flags);
    pElfHasher->Update(desc.topology);
    pApiHasher->Update(desc.primitiveRestartEnable);
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineTessellationStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.patchControlPoints
//     - pDomainOriginStateCreateInfo->domainOrigin
static void GenerateHashFromTessellationStateCreateInfo(
    const VkPipelineTessellationStateCreateInfo& desc,
    Util::MetroHash128*                          pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.patchControlPoints);

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO:
            {
                const auto* pExtInfo = static_cast<const VkPipelineTessellationDomainOriginStateCreateInfo*>(pNext);
                pHasher->Update(pExtInfo->sType);
                pHasher->Update(pExtInfo->domainOrigin);

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineViewportStateCreateInfo struct
// Pipeline compilation affected by: none
static void GenerateHashFromViewportStateCreateInfo(
    const VkPipelineViewportStateCreateInfo& desc,
    const uint64_t                           dynamicStateFlags,
    Util::MetroHash128*                      pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.viewportCount);

    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::Viewport) == false) &&
        (desc.pViewports != nullptr))
    {
        for (uint32_t i = 0; i < desc.viewportCount; i++)
        {
            pHasher->Update(desc.pViewports[i]);
        }
    }

    pHasher->Update(desc.scissorCount);

    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::Scissor) == false) &&
        (desc.pScissors != nullptr))
    {
        for (uint32_t i = 0; i < desc.scissorCount; i++)
        {
            pHasher->Update(desc.pScissors[i]);
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineRasterizationStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.depthClampEnable
//     - desc.rasterizerDiscardEnable
//     - desc.polygonMode
//     - desc.cullMode
//     - desc.frontFace
//     - desc.depthBiasEnable
//     - pStreamCreateInfo->rasterizationStream
static void GenerateHashFromRasterizationStateCreateInfo(
    const VkPipelineRasterizationStateCreateInfo& desc,
    const bool                                    rasterizerDiscardEnableDynamic,
    Util::MetroHash128*                           pElfHasher,
    Util::MetroHash128*                           pApiHasher)
{
    pElfHasher->Update(desc.flags);
    pElfHasher->Update(desc.depthClampEnable);
    pElfHasher->Update(desc.polygonMode);
    pElfHasher->Update(desc.cullMode);
    pElfHasher->Update(desc.frontFace);
    pElfHasher->Update(desc.depthBiasEnable);
    pApiHasher->Update(desc.depthBiasConstantFactor);
    pApiHasher->Update(desc.depthBiasClamp);
    pApiHasher->Update(desc.depthBiasSlopeFactor);
    pApiHasher->Update(desc.lineWidth);

    if (rasterizerDiscardEnableDynamic)
    {
        pApiHasher->Update(desc.rasterizerDiscardEnable);
    }
    else
    {
        pElfHasher->Update(desc.rasterizerDiscardEnable);
    }

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationConservativeStateCreateInfoEXT*>(pNext);
                pApiHasher->Update(pExtInfo->sType);
                pApiHasher->Update(pExtInfo->flags);
                pApiHasher->Update(pExtInfo->conservativeRasterizationMode);
                pApiHasher->Update(pExtInfo->extraPrimitiveOverestimationSize);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationStateRasterizationOrderAMD*>(pNext);
                pApiHasher->Update(pExtInfo->sType);
                pApiHasher->Update(pExtInfo->rasterizationOrder);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationStateStreamCreateInfoEXT*>(pNext);
                pElfHasher->Update(pExtInfo->sType);
                pElfHasher->Update(pExtInfo->flags);
                pElfHasher->Update(pExtInfo->rasterizationStream);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pNext);
                pElfHasher->Update(pExtInfo->sType);
                pElfHasher->Update(pExtInfo->flags);
                pElfHasher->Update(pExtInfo->depthClipEnable);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo =
                    static_cast<const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT*>(pNext);
                pElfHasher->Update(pExtInfo->provokingVertexMode);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationLineStateCreateInfoEXT*>(pNext);
                pElfHasher->Update(pExtInfo->sType);
                pElfHasher->Update(pExtInfo->lineRasterizationMode);
                pElfHasher->Update(pExtInfo->stippledLineEnable);
                pElfHasher->Update(pExtInfo->lineStippleFactor);
                pElfHasher->Update(pExtInfo->lineStipplePattern);

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineMultisampleStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.rasterizationSamples
//     - desc.sampleShadingEnable
//     - desc.minSampleShading
//     - desc.alphaToCoverageEnable
static void GenerateHashFromMultisampleStateCreateInfo(
    const VkPipelineMultisampleStateCreateInfo& desc,
    Util::MetroHash128*                         pElfHasher,
    Util::MetroHash128*                         pApiHasher)
{
    pElfHasher->Update(desc.flags);
    pElfHasher->Update(desc.rasterizationSamples);
    pElfHasher->Update(desc.sampleShadingEnable);
    pElfHasher->Update(desc.minSampleShading);

    if (desc.pSampleMask != nullptr)
    {
        for (uint32_t i = 0; i < ceil(((float)desc.rasterizationSamples) / 32.0f); i++)
        {
            pApiHasher->Update(desc.pSampleMask[i]);
        }
    }

    pElfHasher->Update(desc.alphaToCoverageEnable);
    pApiHasher->Update(desc.alphaToOneEnable);

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(pNext);
                pApiHasher->Update(pExtInfo->sType);
                pApiHasher->Update(pExtInfo->sampleLocationsEnable);
                pApiHasher->Update(pExtInfo->sampleLocationsInfo.sType);
                pApiHasher->Update(pExtInfo->sampleLocationsInfo.sampleLocationsPerPixel);
                pApiHasher->Update(pExtInfo->sampleLocationsInfo.sampleLocationGridSize);
                pApiHasher->Update(pExtInfo->sampleLocationsInfo.sampleLocationsCount);

                for (uint32 i = 0; i < pExtInfo->sampleLocationsInfo.sampleLocationsCount; i++)
                {
                    pApiHasher->Update(pExtInfo->sampleLocationsInfo.pSampleLocations[i]);
                }

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineDepthStencilStateCreateInfo struct
// Pipeline compilation affected by: none
static void GenerateHashFromDepthStencilStateCreateInfo(
    const VkPipelineDepthStencilStateCreateInfo& desc,
    Util::MetroHash128*                          pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.depthTestEnable);
    pHasher->Update(desc.depthWriteEnable);
    pHasher->Update(desc.depthCompareOp);
    pHasher->Update(desc.depthBoundsTestEnable);
    pHasher->Update(desc.stencilTestEnable);
    pHasher->Update(desc.front);
    pHasher->Update(desc.back);
    pHasher->Update(desc.minDepthBounds);
    pHasher->Update(desc.maxDepthBounds);
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineColorBlendStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.pAttachments
static void GenerateHashFromColorBlendStateCreateInfo(
    const VkPipelineColorBlendStateCreateInfo& desc,
    Util::MetroHash128*                        pElfHasher,
    Util::MetroHash128*                        pApiHasher)
{
    pElfHasher->Update(desc.flags);
    pApiHasher->Update(desc.logicOpEnable);
    pApiHasher->Update(desc.logicOp);
    pElfHasher->Update(desc.attachmentCount);

    for (uint32 i = 0; i < desc.attachmentCount; i++)
    {
        pElfHasher->Update(desc.pAttachments[i]);
    }

    pApiHasher->Update(desc.blendConstants);

    if (desc.pNext != nullptr)
    {
        const void* pNext = desc.pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineColorBlendAdvancedStateCreateInfoEXT*>(pNext);
                pApiHasher->Update(pExtInfo->sType);
                pApiHasher->Update(pExtInfo->srcPremultiplied);
                pApiHasher->Update(pExtInfo->dstPremultiplied);
                pApiHasher->Update(pExtInfo->blendOverlap);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_COLOR_WRITE_CREATE_INFO_EXT:
            {
                const auto pExtInfo = static_cast<const VkPipelineColorWriteCreateInfoEXT*>(pNext);
                pApiHasher->Update(pExtInfo->sType);
                pApiHasher->Update(pExtInfo->attachmentCount);

                if (pExtInfo->pColorWriteEnables != nullptr)
                {
                    uint32 count = Util::Min(pExtInfo->attachmentCount, Pal::MaxColorTargets);

                    for (uint32 i = 0; i < count; ++i)
                    {
                        pApiHasher->Update(pExtInfo->pColorWriteEnables[i]);
                    }
                }

                break;
            }
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GenerateHashForVertexInputInterfaceState(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    Util::MetroHash128*                 pElfHasher,
    Util::MetroHash128*                 pApiHasher)
{
    if (pCreateInfo->pVertexInputState != nullptr)
    {
        GenerateHashFromVertexInputStateCreateInfo(*pCreateInfo->pVertexInputState, pElfHasher);
    }

    if (pCreateInfo->pInputAssemblyState != nullptr)
    {
        GenerateHashFromInputAssemblyStateCreateInfo(*pCreateInfo->pInputAssemblyState, pElfHasher, pApiHasher);
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GenerateHashForPreRasterizationShadersState(
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineLibraryInfo*      pLibInfo,
    uint32_t                                dynamicStateFlags,
    Util::MetroHash128*                     pElfHasher,
    Util::MetroHash128*                     pApiHasher)
{
    if ((IsRasterizationDisabled(pCreateInfo, pLibInfo, dynamicStateFlags) == false) &&
        (pCreateInfo->pViewportState != nullptr))
    {
        GenerateHashFromViewportStateCreateInfo(*pCreateInfo->pViewportState, dynamicStateFlags, pApiHasher);
    }

    if (pCreateInfo->pRasterizationState != nullptr)
    {
        bool rasterizerDiscardEnableDynamic =
            IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable);

        GenerateHashFromRasterizationStateCreateInfo(*pCreateInfo->pRasterizationState,
                                                     rasterizerDiscardEnableDynamic,
                                                     pElfHasher,
                                                     pApiHasher);
    }

    const auto activeStages = GetActiveShaderStages(pCreateInfo, pLibInfo);
    if ((activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        && (pCreateInfo->pTessellationState != nullptr))
    {
        GenerateHashFromTessellationStateCreateInfo(*pCreateInfo->pTessellationState, pElfHasher);
    }

    if (pCreateInfo->renderPass != VK_NULL_HANDLE)
    {
        pElfHasher->Update(RenderPass::ObjectFromHandle(pCreateInfo->renderPass)->GetSubpassHash(pCreateInfo->subpass));
    }

    EXTRACT_VK_STRUCTURES_0(
        discardRectangle,
        PipelineDiscardRectangleStateCreateInfoEXT,
        static_cast<const VkPipelineDiscardRectangleStateCreateInfoEXT*>(pCreateInfo->pNext),
        PIPELINE_DISCARD_RECTANGLE_STATE_CREATE_INFO_EXT)

    if (pPipelineDiscardRectangleStateCreateInfoEXT != nullptr)
    {
        pApiHasher->Update(pPipelineDiscardRectangleStateCreateInfoEXT->sType);
        pApiHasher->Update(pPipelineDiscardRectangleStateCreateInfoEXT->flags);
        pApiHasher->Update(pPipelineDiscardRectangleStateCreateInfoEXT->discardRectangleMode);
        pApiHasher->Update(pPipelineDiscardRectangleStateCreateInfoEXT->discardRectangleCount);

        if (pPipelineDiscardRectangleStateCreateInfoEXT->pDiscardRectangles != nullptr)
        {
            for (uint32 i = 0; i < pPipelineDiscardRectangleStateCreateInfoEXT->discardRectangleCount; i++)
            {
                pApiHasher->Update(pPipelineDiscardRectangleStateCreateInfoEXT->pDiscardRectangles[i]);
            }
        }
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GenerateHashForFragmentShaderState(
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    Util::MetroHash128*                     pElfHasher,
    Util::MetroHash128*                     pApiHasher)
{
    if (pCreateInfo->pMultisampleState != nullptr)
    {
        GenerateHashFromMultisampleStateCreateInfo(*pCreateInfo->pMultisampleState, pElfHasher, pApiHasher);
    }

    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pCreateInfo->renderPass);

    EXTRACT_VK_STRUCTURES_0(
        renderingCreateInfo,
        PipelineRenderingCreateInfoKHR,
        static_cast<const VkPipelineRenderingCreateInfoKHR*>(pCreateInfo->pNext),
        PIPELINE_RENDERING_CREATE_INFO_KHR);

    if ((pCreateInfo->pDepthStencilState != nullptr) &&
        (GetDepthFormat(pRenderPass, pCreateInfo->subpass, pPipelineRenderingCreateInfoKHR) != VK_FORMAT_UNDEFINED))
    {
        GenerateHashFromDepthStencilStateCreateInfo(*pCreateInfo->pDepthStencilState, pElfHasher);
    }

    EXTRACT_VK_STRUCTURES_0(
        variableRateShading,
        PipelineFragmentShadingRateStateCreateInfoKHR,
        static_cast<const VkPipelineFragmentShadingRateStateCreateInfoKHR*>(pCreateInfo->pNext),
        PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR)

    if (pPipelineFragmentShadingRateStateCreateInfoKHR != nullptr)
    {
        pApiHasher->Update(pPipelineFragmentShadingRateStateCreateInfoKHR->fragmentSize.width);
        pApiHasher->Update(pPipelineFragmentShadingRateStateCreateInfoKHR->fragmentSize.height);
        pApiHasher->Update(pPipelineFragmentShadingRateStateCreateInfoKHR->combinerOps[0]);
        pApiHasher->Update(pPipelineFragmentShadingRateStateCreateInfoKHR->combinerOps[1]);
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GenerateHashForFragmentOutputInterfaceState(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    Util::MetroHash128*                 pElfHasher,
    Util::MetroHash128*                 pApiHasher)
{
    EXTRACT_VK_STRUCTURES_0(
        renderingCreateInfo,
        PipelineRenderingCreateInfoKHR,
        static_cast<const VkPipelineRenderingCreateInfoKHR*>(pCreateInfo->pNext),
        PIPELINE_RENDERING_CREATE_INFO_KHR);

    uint32 colorAttachmentCount = 0;
    if (pPipelineRenderingCreateInfoKHR != nullptr)
    {
        colorAttachmentCount = pPipelineRenderingCreateInfoKHR->colorAttachmentCount;

        pElfHasher->Update(pPipelineRenderingCreateInfoKHR->viewMask);
        pElfHasher->Update(pPipelineRenderingCreateInfoKHR->colorAttachmentCount);
        for (uint32_t i = 0; i < pPipelineRenderingCreateInfoKHR->colorAttachmentCount; ++i)
        {
            pElfHasher->Update(pPipelineRenderingCreateInfoKHR->pColorAttachmentFormats[i]);
        }
        pElfHasher->Update(pPipelineRenderingCreateInfoKHR->depthAttachmentFormat);
        pElfHasher->Update(pPipelineRenderingCreateInfoKHR->stencilAttachmentFormat);
    }
    else if (pCreateInfo->renderPass != VK_NULL_HANDLE)
    {
        const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pCreateInfo->renderPass);
        colorAttachmentCount = pRenderPass->GetSubpassColorReferenceCount(pCreateInfo->subpass);

        pElfHasher->Update(RenderPass::ObjectFromHandle(pCreateInfo->renderPass)->GetSubpassHash(pCreateInfo->subpass));
    }

    if ((pCreateInfo->pColorBlendState != nullptr) && (colorAttachmentCount != 0))
    {
        GenerateHashFromColorBlendStateCreateInfo(*pCreateInfo->pColorBlendState, pElfHasher, pApiHasher);
    }

    if (pCreateInfo->pMultisampleState != nullptr)
    {
        GenerateHashFromMultisampleStateCreateInfo(*pCreateInfo->pMultisampleState, pElfHasher, pApiHasher);
    }
}

// =====================================================================================================================
// Checks if rasterization is dynamically disabled
bool GraphicsPipelineCommon::IsRasterizationDisabled(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const GraphicsPipelineLibraryInfo*  pLibInfo,
    uint64_t                            dynamicStateFlags)
{
    bool disableRasterization = false;

    if (pLibInfo->libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        if ((pCreateInfo->pRasterizationState != nullptr) &&
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable) == false))
        {
            disableRasterization = pCreateInfo->pRasterizationState->rasterizerDiscardEnable;
        }
    }
    else if (pLibInfo->pPreRasterizationShaderLib != nullptr)
    {
        disableRasterization =
            pLibInfo->pPreRasterizationShaderLib->GetPipelineObjectCreateInfo().immedInfo.rasterizerDiscardEnable;
    }

    return disableRasterization;
}

// =====================================================================================================================
// Generates the API PSO hash using the contents of the VkGraphicsPipelineCreateInfo struct
// Pipeline compilation affected by:
//     - pCreateInfo->pStages
//     - pCreateInfo->pVertexInputState
//     - pCreateInfo->pInputAssemblyState
//     - pCreateInfo->pTessellationState
//     - pCreateInfo->pRasterizationState
//     - pCreateInfo->pMultisampleState
//     - pCreateInfo->pColorBlendState
//     - pCreateInfo->layout
//     - pCreateInfo->renderPass
//     - pCreateInfo->subpass
void GraphicsPipelineCommon::BuildApiHash(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    PipelineCreateFlags                 flags,
    uint64_t*                           pApiHash,
    Util::MetroHash::Hash*              pElfHash)
{
    Util::MetroHash128 elfHasher;
    Util::MetroHash128 apiHasher;

    GraphicsPipelineLibraryInfo libInfo;
    GraphicsPipelineCommon::ExtractLibraryInfo(pCreateInfo, flags, &libInfo);

    uint64_t dynamicStateFlags = GetDynamicStateFlags(pCreateInfo->pDynamicState, &libInfo);
    elfHasher.Update(dynamicStateFlags);

    // Hash only flags needed for pipeline caching
    elfHasher.Update(GetCacheIdControlFlags(flags));

    // Hash flags not accounted for in the elf hash
    apiHasher.Update(flags);

    if (pCreateInfo->layout != VK_NULL_HANDLE)
    {
        elfHasher.Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());
    }

    for (uint32_t i = 0; i < pCreateInfo->stageCount; ++i)
    {
        GenerateHashFromShaderStageCreateInfo(pCreateInfo->pStages[i], &elfHasher);
    }

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT)
    {
        GenerateHashForVertexInputInterfaceState(pCreateInfo, &elfHasher, &apiHasher);
    }
    else if (libInfo.pVertexInputInterfaceLib != nullptr)
    {
        elfHasher.Update(*libInfo.pVertexInputInterfaceLib->GetElfHash());
        apiHasher.Update(libInfo.pVertexInputInterfaceLib->GetApiHash());
    }

    if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT)
    {
        GenerateHashForPreRasterizationShadersState(
            pCreateInfo, &libInfo, dynamicStateFlags, &elfHasher, &apiHasher);
    }
    else if (libInfo.pPreRasterizationShaderLib != nullptr)
    {
        elfHasher.Update(*libInfo.pPreRasterizationShaderLib->GetElfHash());
        apiHasher.Update(libInfo.pPreRasterizationShaderLib->GetApiHash());
    }

    const bool enableRasterization =
        (~libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) ||
        (IsRasterizationDisabled(pCreateInfo, &libInfo, dynamicStateFlags) == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnable);

    if (enableRasterization)
    {
        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT)
        {
            GenerateHashForFragmentShaderState(pCreateInfo, &elfHasher, &apiHasher);
        }
        else if (libInfo.pFragmentShaderLib != nullptr)
        {
            elfHasher.Update(*libInfo.pFragmentShaderLib->GetElfHash());
            apiHasher.Update(libInfo.pFragmentShaderLib->GetApiHash());
        }

        if (libInfo.libFlags & VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
        {
            GenerateHashForFragmentOutputInterfaceState(pCreateInfo, &elfHasher, &apiHasher);
        }
        else if (libInfo.pFragmentOutputInterfaceLib != nullptr)
        {
            elfHasher.Update(*libInfo.pFragmentOutputInterfaceLib->GetElfHash());
            apiHasher.Update(libInfo.pFragmentOutputInterfaceLib->GetApiHash());
        }
    }

    if ((flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) && (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(GraphicsPipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);

    elfHasher.Finalize(reinterpret_cast<uint8_t*>(pElfHash));

    Util::MetroHash::Hash apiHashFull;
    apiHasher.Update(*pElfHash);
    apiHasher.Finalize(reinterpret_cast<uint8_t*>(&apiHashFull));

    *pApiHash = Util::MetroHash::Compact64(&apiHashFull);
}

}
