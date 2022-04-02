/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_pipeline_layout.h"
#include "include/vk_render_pass.h"

namespace vk
{

// We need to make sure that the number of dynamic states should not be larger than 32.
// Otherwise, we cannot represent the collection of them by a uint32.
static_assert(static_cast<uint32_t>(DynamicStatesInternal::DynamicStatesInternalCount) <= 32,
              "Unexpected enum count: DynamicStatesInternal");

// =====================================================================================================================
// The dynamic states of Vertex Input Interface section
// - VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT
// - VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT
// - VK_DYNAMIC_STATE_VERTEX_INPUT_EXT (not available)
// - VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT
constexpr uint32_t ViiDynamicStatesMask = 0
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnableExt));

// =====================================================================================================================
// The dynamic states of Pre-Rasterization Shaders section
// - VK_DYNAMIC_STATE_VIEWPORT
// - VK_DYNAMIC_STATE_SCISSOR
// - VK_DYNAMIC_STATE_LINE_WIDTH
// - VK_DYNAMIC_STATE_DEPTH_BIAS
// - VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV (not available)
// - VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV (not available)
// - VK_DYNAMIC_STATE_LINE_STIPPLE_EXT
// - VK_DYNAMIC_STATE_CULL_MODE_EXT
// - VK_DYNAMIC_STATE_FRONT_FACE_EXT
// - VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT
// - VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT
// - VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT (not available)
// - VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT
// - VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT
constexpr uint32_t PrsDynamicStatesMask = 0
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::Viewport))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::Scissor))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::LineWidth))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBias))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::CullModeExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnableExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnableExt));

// =====================================================================================================================
// The dynamic states of Fragment Shader (Post-Rasterization) section
// - VK_DYNAMIC_STATE_DEPTH_BOUNDS
// - VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK
// - VK_DYNAMIC_STATE_STENCIL_WRITE_MASK
// - VK_DYNAMIC_STATE_STENCIL_REFERENCE
// - VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR
// - VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT
// - VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT
// - VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT
// - VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT
// - VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT
// - VK_DYNAMIC_STATE_STENCIL_OP_EXT
constexpr uint32_t FgsDynamicStatesMask = 0
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilReference))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt));

// =====================================================================================================================
// The dynamic states of Fragment Output Interface section
// - VK_DYNAMIC_STATE_BLEND_CONSTANTS
// - VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT
// - VK_DYNAMIC_STATE_LOGIC_OP_EXT (not available)
// - VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT
constexpr uint32_t FoiDynamicStatesMask = 0
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt))
    | (1 << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnableExt));

// =====================================================================================================================
// Helper function used to check whether a specific dynamic state is set
static bool IsDynamicStateEnabled(const uint32_t dynamicStateFlags, const DynamicStatesInternal internalState)
{
    return dynamicStateFlags & (1 << static_cast<uint32_t>(internalState));
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
    Pal::ColorBlendStateCreateInfo*            pInfo)
{
    const uint32_t numColorTargets = Min(pColorBlendState->attachmentCount, Pal::MaxColorTargets);

    for (uint32_t i = 0; i < numColorTargets; ++i)
    {
        const VkPipelineColorBlendAttachmentState& attachmentState = pColorBlendState->pAttachments[i];
        auto pBlendDst = &pInfo->targets[i];

        pBlendDst->blendEnable    = (attachmentState.blendEnable == VK_TRUE);
        pBlendDst->srcBlendColor  = VkToPalBlend(attachmentState.srcColorBlendFactor);
        pBlendDst->dstBlendColor  = VkToPalBlend(attachmentState.dstColorBlendFactor);
        pBlendDst->blendFuncColor = VkToPalBlendFunc(attachmentState.colorBlendOp);
        pBlendDst->srcBlendAlpha  = VkToPalBlend(attachmentState.srcAlphaBlendFactor);
        pBlendDst->dstBlendAlpha  = VkToPalBlend(attachmentState.dstAlphaBlendFactor);
        pBlendDst->blendFuncAlpha = VkToPalBlendFunc(attachmentState.alphaBlendOp);
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
        BuildPalColorBlendStateCreateInfo(pColorBlendState, &palInfo);
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

    VK_ASSERT(rasterizationSampleCount == coverageSampleCount);

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
VkShaderStageFlagBits GraphicsPipelineCommon::GetActiveShaderStages(
    const VkGraphicsPipelineCreateInfo*  pGraphicsPipelineCreateInfo
    )
{
    VK_ASSERT(pGraphicsPipelineCreateInfo != nullptr);

    VkShaderStageFlagBits activeStages = static_cast<VkShaderStageFlagBits>(0);

    for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
    {
        activeStages = static_cast<VkShaderStageFlagBits>(activeStages | pGraphicsPipelineCreateInfo->pStages[i].stage);
    }

    return activeStages;
}

// =====================================================================================================================
uint32_t GraphicsPipelineCommon::GetDynamicStateFlags(
    const VkPipelineDynamicStateCreateInfo* pDy
    )
{
    uint32_t dynamicState = 0;

    // The section of the following dynamic states are not defined, so we don't get them from libraries
    // - VK_DYNAMIC_STATE_WAVE_LIMIT_AMD
    // - VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV (not available)
    // - VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT (not available)
    // - VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV (not available)

    // Get dynamic states from VkPipelineDynamicStateCreateInfo
    if (pDy != nullptr)
    {
        const uint32_t viiMask = 0xFFFFFFFF;
        const uint32_t prsMask = 0xFFFFFFFF;
        const uint32_t fgsMask = 0xFFFFFFFF;
        const uint32_t foiMask = 0xFFFFFFFF;

        for (uint32_t i = 0; i < pDy->dynamicStateCount; ++i)
        {
            switch (static_cast<uint32_t>(pDy->pDynamicStates[i]))
            {
            case VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
                dynamicState |= viiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt));
                break;
            case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
                dynamicState |= viiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt));
                break;
            case VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT:
                dynamicState |= viiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnableExt));
                break;
            case VK_DYNAMIC_STATE_VIEWPORT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::Viewport));
                break;
            case VK_DYNAMIC_STATE_SCISSOR:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::Scissor));
                break;
            case VK_DYNAMIC_STATE_LINE_WIDTH:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::LineWidth));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BIAS:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBias));
                break;
            case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt));
                break;
            case VK_DYNAMIC_STATE_CULL_MODE_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::CullModeExt));
                break;
            case VK_DYNAMIC_STATE_FRONT_FACE_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt));
                break;
            case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount));
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::Viewport));
                break;
            case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount));
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::Scissor));
                break;
            case VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnableExt));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT:
                dynamicState |= prsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnableExt));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds));
                break;
            case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask));
                break;
            case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask));
                break;
            case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilReference));
                break;
            case VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr));
                break;
            case VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt));
                break;
            case VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt));
                break;
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt));
                break;
            case VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt));
                break;
            case VK_DYNAMIC_STATE_STENCIL_OP_EXT:
                dynamicState |= fgsMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt));
                break;
            case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
                dynamicState |= foiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants));
                break;
            case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
                dynamicState |= foiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt));
                break;
            case VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT:
                dynamicState |= foiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt));
                break;
            case VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT:
                dynamicState |= foiMask & (1 << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnableExt));
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
VkResult GraphicsPipelineCommon::Create(
    Device*                             pDevice,
    PipelineCache*                      pPipelineCache,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    VkPipeline*                         pPipeline)
{
    VkResult result;

    {
        result =  GraphicsPipeline::Create(
            pDevice, pPipelineCache, pCreateInfo, pAllocator, pPipeline);
    }

    return result;
}

// =====================================================================================================================
static void BuildRasterizationState(
    const Device*                                 pDevice,
    const VkPipelineRasterizationStateCreateInfo* pRs,
    const uint32_t                                dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*             pInfo)
{
    if (pRs != nullptr)
    {
        VK_ASSERT(pRs->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);

        // By default rasterization is disabled, unless rasterization creation info is present

        const RuntimeSettings&        settings        = pDevice->GetRuntimeSettings();
        const PhysicalDevice*         pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
        const VkPhysicalDeviceLimits& limits          = pPhysicalDevice->GetLimits();

        // Enable perpendicular end caps if we report strictLines semantics
        pInfo->pipeline.rsState.perpLineEndCapsEnable = (limits.strictLines == VK_TRUE);

        pInfo->pipeline.viewportInfo.depthClipNearEnable            = (pRs->depthClampEnable == VK_FALSE);
        pInfo->pipeline.viewportInfo.depthClipFarEnable             = (pRs->depthClampEnable == VK_FALSE);

        pInfo->immedInfo.triangleRasterState.frontFillMode          = VkToPalFillMode(pRs->polygonMode);
        pInfo->immedInfo.triangleRasterState.backFillMode           = VkToPalFillMode(pRs->polygonMode);
        pInfo->immedInfo.triangleRasterState.cullMode               = VkToPalCullMode(pRs->cullMode);
        pInfo->immedInfo.triangleRasterState.frontFace              = VkToPalFaceOrientation(pRs->frontFace);

        pInfo->immedInfo.triangleRasterState.flags.depthBiasEnable  = pRs->depthBiasEnable;
        pInfo->immedInfo.depthBiasParams.depthBias                  = pRs->depthBiasConstantFactor;
        pInfo->immedInfo.depthBiasParams.depthBiasClamp             = pRs->depthBiasClamp;
        pInfo->immedInfo.depthBiasParams.slopeScaledDepthBias       = pRs->depthBiasSlopeFactor;

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt) == true)
        {
            pInfo->immedInfo.rasterizerDiscardEnable = false;
        }
        else
        {
            pInfo->immedInfo.rasterizerDiscardEnable = pRs->rasterizerDiscardEnable;
        }

        if ((pRs->depthBiasEnable ||
             IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnableExt)) &&
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBias) == false))
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBias);
        }

        // point size must be set via gl_PointSize, otherwise it must be 1.0f.
        constexpr float DefaultPointSize = 1.0f;

        pInfo->immedInfo.pointLineRasterParams.lineWidth    = pRs->lineWidth;
        pInfo->immedInfo.pointLineRasterParams.pointSize    = DefaultPointSize;
        pInfo->immedInfo.pointLineRasterParams.pointSizeMin = limits.pointSizeRange[0];
        pInfo->immedInfo.pointLineRasterParams.pointSizeMax = limits.pointSizeRange[1];

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineWidth) == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::LineWidth);
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
                            pInfo->msaa.flags.enableConservativeRasterization = false;
                        }
                        break;
                    case VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT:
                        {
                            pInfo->msaa.flags.enableConservativeRasterization = true;
                            pInfo->msaa.conservativeRasterizationMode =
                                Pal::ConservativeRasterizationMode::Overestimate;
                        }
                        break;
                    case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
                        {
                            pInfo->msaa.flags.enableConservativeRasterization = true;
                            pInfo->msaa.conservativeRasterizationMode =
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

                    pInfo->msaa.flags.enableLineStipple                 = pRsLine->stippledLineEnable;

                    pInfo->immedInfo.lineStippleParams.lineStippleScale = (pRsLine->lineStippleFactor - 1);
                    pInfo->immedInfo.lineStippleParams.lineStippleValue = pRsLine->lineStipplePattern;

                    if (pRsLine->stippledLineEnable &&
                        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::LineStippleExt) == false))
                    {
                        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt);
                    }
                }
                break;
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsDepthClip =
                        static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pNext);

                    pInfo->pipeline.viewportInfo.depthClipNearEnable = (pRsDepthClip->depthClipEnable == VK_TRUE);
                    pInfo->pipeline.viewportInfo.depthClipFarEnable  = (pRsDepthClip->depthClipEnable == VK_TRUE);
                }
                break;
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT:
                {
                    const auto* pRsProvokingVertex =
                        static_cast<const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT*>(pNext);
                    pInfo->immedInfo.triangleRasterState.provokingVertex =
                        static_cast<Pal::ProvokingVertex>(pRsProvokingVertex->provokingVertexMode);

                    static_assert(static_cast<Pal::ProvokingVertex>(VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT) ==
                                  Pal::ProvokingVertex::First,
                                  "VK and PAL enums don't match");
                    static_assert(static_cast<Pal::ProvokingVertex>(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT) ==
                                  Pal::ProvokingVertex::Last,
                                  "VK and PAL enums don't match");
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
        switch (settings.pipelineBinningMode)
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
}

// =====================================================================================================================
static void BuildViewportState(
    const Device*                            pDevice,
    const VkPipelineViewportStateCreateInfo* pVp,
    const uint32_t                           dynamicStateFlags,
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
        pInfo->pipeline.viewportInfo.depthRange =
            ((pPipelineViewportDepthClipControlCreateInfoEXT != nullptr) &&
            (pPipelineViewportDepthClipControlCreateInfoEXT->negativeOneToOne == VK_TRUE)) ?
            Pal::DepthRange::NegativeOneToOne : Pal::DepthRange::ZeroToOne;

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
            const bool khrMaintenance1    = (enabledApiVersion >= VK_MAKE_VERSION(1, 1, 0)) || maintenanceEnabled;

            for (uint32_t i = 0; i < pVp->viewportCount; ++i)
            {
                VkToPalViewport(pVp->pViewports[i],
                        i,
                        khrMaintenance1,
                        &pInfo->immedInfo.viewportParams);
            }

            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::Viewport);
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::Scissor) == false)
        {
            VK_ASSERT(pVp->pScissors != nullptr);

            for (uint32_t i = 0; i < pVp->scissorCount; ++i)
            {
                VkToPalScissorRect(pVp->pScissors[i], i, &pInfo->immedInfo.scissorRectParams);
            }
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::Scissor);
        }
    }
}

// =====================================================================================================================
static void BuildVrsRateParams(
    const Device*                                          pDevice,
    const VkPipelineFragmentShadingRateStateCreateInfoKHR* pFsr,
    const uint32_t                                         dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*                      pInfo)
{
    if ((IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FragmentShadingRateStateKhr) == false) &&
        (pFsr != nullptr))
    {
        pInfo->immedInfo.vrsRateParams.flags.exposeVrsPixelsMask = 1;

        pInfo->immedInfo.vrsRateParams.shadingRate =
            VkToPalShadingSize(VkClampShadingRate(pFsr->fragmentSize, pDevice->GetMaxVrsShadingRate()));

        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::ProvokingVertex)] =
            VkToPalShadingRateCombinerOp(pFsr->combinerOps[0]);

        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::Primitive)]= Pal::VrsCombiner::Passthrough;

        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::Image)] = VkToPalShadingRateCombinerOp(pFsr->combinerOps[1]);

        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::PsIterSamples)] = Pal::VrsCombiner::Passthrough;

        pInfo->staticStateMask |=
            1 << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr);
    }
}

// =====================================================================================================================

// =====================================================================================================================
static void BuildMultisampleState(
    const VkPipelineMultisampleStateCreateInfo* pMs,
    const RenderPass*                           pRenderPass,
    const uint32_t                              subpass,
    const uint32_t                              dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*           pInfo)
{
    if (pMs != nullptr)
    {
        pInfo->flags.customMultiSampleState = true;
        pInfo->flags.force1x1ShaderRate =
            (pMs->sampleShadingEnable || (pMs->rasterizationSamples == VK_SAMPLE_COUNT_8_BIT));
        pInfo->flags.sampleShadingEnable = pMs->sampleShadingEnable;

        // Sample Locations
        EXTRACT_VK_STRUCTURES_0(
            SampleLocations,
            PipelineSampleLocationsStateCreateInfoEXT,
            static_cast<const VkPipelineSampleLocationsStateCreateInfoEXT*>(pMs->pNext),
            PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT)

        pInfo->flags.customSampleLocations = ((pPipelineSampleLocationsStateCreateInfoEXT != nullptr) &&
                                              (pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsEnable));

        uint32_t subpassCoverageSampleCount;
        uint32_t subpassColorSampleCount;
        uint32_t subpassDepthSampleCount;
        GraphicsPipelineCommon::GetSubpassSampleCount(
            pMs, pRenderPass, subpass,
            &subpassCoverageSampleCount, &subpassColorSampleCount, &subpassDepthSampleCount);

        pInfo->msaa.coverageSamples = subpassCoverageSampleCount;
        pInfo->msaa.exposedSamples  = subpassCoverageSampleCount;

        if (pMs->sampleShadingEnable && (pMs->minSampleShading > 0.0f))
        {
            pInfo->msaa.pixelShaderSamples =
                Pow2Pad(static_cast<uint32_t>(ceil(subpassColorSampleCount * pMs->minSampleShading)));
        }
        else
        {
            pInfo->msaa.pixelShaderSamples = 1;
        }

        pInfo->msaa.depthStencilSamples = subpassDepthSampleCount;
        pInfo->msaa.shaderExportMaskSamples = subpassCoverageSampleCount;
        pInfo->msaa.sampleMask = (pMs->pSampleMask != nullptr)
                                    ? pMs->pSampleMask[0]
                                    : 0xffffffff;
        pInfo->msaa.sampleClusters         = subpassCoverageSampleCount;
        pInfo->msaa.alphaToCoverageSamples = subpassCoverageSampleCount;
        pInfo->msaa.occlusionQuerySamples  = subpassDepthSampleCount;
        pInfo->sampleCoverage              = subpassCoverageSampleCount;

        pInfo->pipeline.cbState.target[0].forceAlphaToOne = (pMs->alphaToOneEnable == VK_TRUE);
        pInfo->pipeline.cbState.alphaToCoverageEnable     = (pMs->alphaToCoverageEnable == VK_TRUE);

        if (pInfo->flags.customSampleLocations)
        {
            // Enable single-sampled custom sample locations if necessary
            pInfo->msaa.flags.enable1xMsaaSampleLocations = (pInfo->msaa.coverageSamples == 1);

            if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::SampleLocationsExt) == false)
            {
                // We store the custom sample locations if custom sample locations are enabled and the
                // sample locations state is static.
                pInfo->immedInfo.samplePattern.sampleCount =
                    (uint32_t)pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationsPerPixel;

                ConvertToPalMsaaQuadSamplePattern(
                    &pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo,
                    &pInfo->immedInfo.samplePattern.locations);

                VK_ASSERT(pInfo->immedInfo.samplePattern.sampleCount ==
                          static_cast<uint32_t>(pMs->rasterizationSamples));

                pInfo->staticStateMask |=
                    (1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt));
            }
        }
        else
        {
            // We store the standard sample locations if custom sample locations are not enabled.
            pInfo->immedInfo.samplePattern.sampleCount = pMs->rasterizationSamples;
            pInfo->immedInfo.samplePattern.locations =
                *Device::GetDefaultQuadSamplePattern(pMs->rasterizationSamples);

            pInfo->staticStateMask |=
                1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt);
        }
    }
}

// =====================================================================================================================
static void BuildDepthStencilState(
    const VkPipelineDepthStencilStateCreateInfo* pDs,
    const uint32_t                               dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*            pInfo)
{
    if (pDs != nullptr)
    {
        pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = (pDs->stencilTestEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = (pDs->depthTestEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = (pDs->depthWriteEnable == VK_TRUE);
        pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = VkToPalCompareFunc(pDs->depthCompareOp);
        pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = (pDs->depthBoundsTestEnable == VK_TRUE);

        if ((pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable ||
             IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnableExt)) &&
            (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBounds) == false))
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds);
        }

        // According to Graham, we should program the stencil state at PSO bind time,
        // regardless of whether this PSO enables\disables Stencil. This allows a second PSO
        // to inherit the first PSO's settings
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilCompareMask) == false)
        {
            pInfo->staticStateMask |= 1 <<  static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask);
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilWriteMask) == false)
        {
            pInfo->staticStateMask |= 1 <<  static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask);
        }

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilReference) == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::StencilReference);
        }

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
}

// =====================================================================================================================
static void BuildColorBlendState(
    const Device*                              pDevice,
    const VkPipelineRenderingCreateInfoKHR*    pRendering,
    const VkPipelineColorBlendStateCreateInfo* pCb,
    const RenderPass*                          pRenderPass,
    const uint32_t                             subpass,
    const uint32_t                             dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*          pInfo)
{
    bool blendingEnabled = false;
    bool dualSourceBlend = false;

    if (pCb != nullptr)
    {
        pInfo->pipeline.cbState.logicOp = (pCb->logicOpEnable) ?
                                           VkToPalLogicOp(pCb->logicOp) :
                                           Pal::LogicOp::Copy;

        const uint32_t numColorTargets = Min(pCb->attachmentCount, Pal::MaxColorTargets);

        const VkPipelineColorWriteCreateInfoEXT* pColorWriteCreateInfo = nullptr;

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

        for (uint32_t i = 0; i < numColorTargets; ++i)
        {
            const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];

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
                if ((pColorWriteCreateInfo != nullptr) && (pColorWriteCreateInfo->pColorWriteEnables != nullptr) &&
                    (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteEnableExt) == false))
                {
                    if (pColorWriteCreateInfo->pColorWriteEnables[i])
                    {
                        pCbDst->channelWriteMask = src.colorWriteMask;
                    }
                    else
                    {
                        pCbDst->channelWriteMask = 0;
                    }
                }
                else
                {
                    pCbDst->channelWriteMask = src.colorWriteMask;
                }

                blendingEnabled |= (src.blendEnable == VK_TRUE);
            }
        }

        BuildPalColorBlendStateCreateInfo(pCb, &pInfo->blend);
        dualSourceBlend = GraphicsPipelineCommon::GetDualSourceBlendEnableState(pDevice, pCb, &pInfo->blend);
    }

    pInfo->pipeline.cbState.dualSourceBlendEnable = dualSourceBlend;

    if ((blendingEnabled == true) &&
        (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::BlendConstants) == false))
    {
        static_assert(sizeof(pInfo->immedInfo.blendConstParams) == sizeof(pCb->blendConstants),
            "Blend constant structure size mismatch!");

        memcpy(&pInfo->immedInfo.blendConstParams, pCb->blendConstants, sizeof(pCb->blendConstants));

        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::BlendConstants);
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
    const uint32_t                      dynamicStateFlags,
    const bool                          isLibrary,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    const VkPipelineInputAssemblyStateCreateInfo* pIa = pIn->pInputAssemblyState;

    {
        // According to the spec this should never be null except when mesh is enabled
        VK_ASSERT((pIa != nullptr) || (isLibrary == true));

        pInfo->immedInfo.inputAssemblyState.primitiveRestartEnable = (pIa->primitiveRestartEnable != VK_FALSE);
        pInfo->immedInfo.inputAssemblyState.primitiveRestartIndex  = 0xFFFFFFFF;
        pInfo->immedInfo.inputAssemblyState.topology               = VkToPalPrimitiveTopology(pIa->topology);

        pInfo->pipeline.iaState.vertexBufferCount = pVbInfo->bindingTableSize;

        pInfo->pipeline.iaState.topologyInfo.primitiveType = VkToPalPrimitiveType(pIa->topology);

        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopologyExt) == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt);
        }
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::VertexInputBindingStrideExt) == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt);
        }
        if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveRestartEnableExt) == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveRestartEnableExt);
        }
    }
}

// =====================================================================================================================
static void BuildPreRasterizationShaderState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    // Build states via VkPipelineRasterizationStateCreateInfo
    BuildRasterizationState(pDevice, pIn->pRasterizationState, dynamicStateFlags, pInfo);

    if (pInfo->immedInfo.rasterizerDiscardEnable == false)
    {
        // Build states via VkPipelineViewportStateCreateInfo
        BuildViewportState(pDevice, pIn->pViewportState, dynamicStateFlags, pInfo);
    }

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::CullModeExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::CullModeExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FrontFaceExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ViewportCount) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ScissorCount) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnableExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBiasEnableExt);
    }
}

// =====================================================================================================================
static void BuildFragmentShaderState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{
    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pIn->renderPass);
    const uint32_t    subpass     = pIn->subpass;

    // Build states via VkPipelineDepthStencilStateCreateInfo
    BuildDepthStencilState(pIn->pDepthStencilState, dynamicStateFlags, pInfo);

    // Build VRS state
    EXTRACT_VK_STRUCTURES_0(
        variableRateShading,
        PipelineFragmentShadingRateStateCreateInfoKHR,
        static_cast<const VkPipelineFragmentShadingRateStateCreateInfoKHR*>(pIn->pNext),
        PIPELINE_FRAGMENT_SHADING_RATE_STATE_CREATE_INFO_KHR)

    BuildVrsRateParams(pDevice, pPipelineFragmentShadingRateStateCreateInfoKHR, dynamicStateFlags, pInfo);

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthTestEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthCompareOpExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilTestEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilOpExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt);
    }
}

// =====================================================================================================================
static void BuildFragmentOutputInterfaceState(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const uint32_t                      dynamicStateFlags,
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

    if (GetColorAttachmentCount(pRenderPass, subpass, pPipelineRenderingCreateInfoKHR) != 0)
    {
        // Build states via VkPipelineColorBlendStateCreateInfo
        BuildColorBlendState(
        pDevice,
        pPipelineRenderingCreateInfoKHR,
        pIn->pColorBlendState,
        pRenderPass,
        subpass,
        dynamicStateFlags,
        pInfo);
    }

    BuildRenderingState(pDevice,
                        pPipelineRenderingCreateInfoKHR,
                        pIn->pColorBlendState,
                        pRenderPass,
                        pInfo);

    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthWriteEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt);
    }
    if (IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::ColorWriteEnableExt) == false)
    {
        pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::ColorWriteEnableExt);
    }
}

// =====================================================================================================================
static void BuildExecutablePipelineState(
    const GraphicsPipelineBinaryInfo*   pBinInfo,
    const uint32_t                      dynamicStateFlags,
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

        memset(&(pInfo->immedInfo.vrsRateParams),     0, sizeof(pInfo->immedInfo.vrsRateParams));
        memset(&(pInfo->immedInfo.viewportParams),    0, sizeof(pInfo->immedInfo.viewportParams));
        memset(&(pInfo->immedInfo.scissorRectParams), 0, sizeof(pInfo->immedInfo.scissorRectParams));
        memset(&(pInfo->pipeline.cbState.target[0]),  0, sizeof(pInfo->pipeline.cbState.target));
        memset(&(pInfo->blend.targets[0]),            0, sizeof(pInfo->blend.targets));

        pInfo->staticStateMask &=
            ~((1 << static_cast<uint32_t>(DynamicStatesInternal::FragmentShadingRateStateKhr)) |
              (1 << static_cast<uint32_t>(DynamicStatesInternal::Viewport)) |
              (1 << static_cast<uint32_t>(DynamicStatesInternal::Scissor)));
    }

    if (pInfo->dbFormat == VK_FORMAT_UNDEFINED)
    {
        pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = false;
        pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = false;
        pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = Pal::CompareFunc::Always;
        pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = false;
        pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = false;

        pInfo->staticStateMask &=
            ~((1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBounds)) |
              (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask)) |
              (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask)) |
              (1 << static_cast<uint32_t>(DynamicStatesInternal::StencilReference)));
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
        pInfo->msaa.coverageSamples         = 1;
        pInfo->msaa.exposedSamples          = 0;
        pInfo->msaa.pixelShaderSamples      = 1;
        pInfo->msaa.depthStencilSamples     = 1;
        pInfo->msaa.shaderExportMaskSamples = 1;
        pInfo->msaa.sampleMask              = 1;
        pInfo->msaa.sampleClusters          = 1;
        pInfo->msaa.alphaToCoverageSamples  = 1;
        pInfo->msaa.occlusionQuerySamples   = 1;

        pInfo->sampleCoverage = 1;

        pInfo->immedInfo.samplePattern = {};

        pInfo->flags.sampleShadingEnable = false;

        pInfo->staticStateMask &=
            ~(1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt));
    }

#if PAL_BUILD_GFX103
    // Both MSAA and VRS would utilize the value of PS_ITER_SAMPLES
    // Thus, choose the min combiner (i.e. choose the higher quality rate) when both features are enabled
    if ((pInfo->msaa.pixelShaderSamples > 1) &&
        (pInfo->immedInfo.vrsRateParams.flags.exposeVrsPixelsMask == 1))
    {
        pInfo->immedInfo.vrsRateParams.combinerState[
            static_cast<uint32_t>(Pal::VrsCombinerStage::PsIterSamples)] = Pal::VrsCombiner::Min;
    }
#endif

    pInfo->flags.bindDepthStencilObject =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilOpExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilTestEnableExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBoundsTestEnableExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthCompareOpExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthWriteEnableExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthTestEnableExt));

    pInfo->flags.bindTriangleRasterState =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::CullModeExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::FrontFaceExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::DepthBiasEnableExt));

    pInfo->flags.bindStencilRefMasks =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilCompareMask) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilWriteMask) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::StencilReference));

    pInfo->flags.bindInputAssemblyState =
        !(IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveTopologyExt) ||
          IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::PrimitiveRestartEnableExt));
}

// =====================================================================================================================
void GraphicsPipelineCommon::BuildPipelineObjectCreateInfo(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const VbBindingInfo*                pVbInfo,
    const GraphicsPipelineBinaryInfo*   pBinInfo,
    const PipelineLayout*               pPipelineLayout,
    GraphicsPipelineObjectCreateInfo*   pInfo)
{

    pInfo->activeStages = GetActiveShaderStages(pIn
                                                );

    uint32_t dynamicStateFlags = GetDynamicStateFlags(
                                     pIn->pDynamicState
                                     );

    BuildVertexInputInterfaceState(pDevice, pIn, pVbInfo, dynamicStateFlags, false, pInfo);

    BuildPreRasterizationShaderState(pDevice,
                                     pIn,
                                     dynamicStateFlags,
                                     pInfo);

    const bool enableRasterization =
        (pInfo->immedInfo.rasterizerDiscardEnable == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt);

    if (enableRasterization)
    {
        BuildFragmentShaderState(pDevice,
                                 pIn,
                                 dynamicStateFlags,
                                 pInfo);

        BuildFragmentOutputInterfaceState(pDevice, pIn, dynamicStateFlags, pInfo);
    }

    {
        BuildExecutablePipelineState(pBinInfo, dynamicStateFlags, pInfo);

        if ((pBinInfo != nullptr) && (pBinInfo->pOptimizerKey != nullptr))
        {
            pDevice->GetShaderOptimizer()->OverrideGraphicsPipelineCreateInfo(
                *pBinInfo->pOptimizerKey,
                pInfo->activeStages,
                &pInfo->pipeline,
                &pInfo->immedInfo.graphicsShaderInfos);
        }
    }
}

// =====================================================================================================================
// Achieve pipeline layout from VkGraphicsPipelineCreateInfo.
// If the pipeline layout is merged, callee must destroy it manually.
VkResult GraphicsPipelineCommon::AchievePipelineLayout(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*        pAllocator,
    PipelineLayout**                    ppPipelineLayout,
    bool*                               pIsMerged)
{
    VkResult result = VK_SUCCESS;

    *pIsMerged = false;

    {
        *ppPipelineLayout = PipelineLayout::ObjectFromHandle(pCreateInfo->layout);
    }

    return result;
}

// =====================================================================================================================
// Build the input information for pipeline compiler to create the graphics pipeline binaries
VkResult GraphicsPipelineCommon::BuildPipelineBinaryCreateInfo(
    const Device*                       pDevice,
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const PipelineLayout*               pPipelineLayout,
    PipelineCache*                      pPipelineCache,
    GraphicsPipelineBinaryCreateInfo*   pBinInfo,
    GraphicsPipelineShaderStageInfo*    pShaderInfo,
    VbBindingInfo*                      pVbInfo,
    PipelineInternalBufferInfo*         pInternalBufferInfo,
    ShaderModuleHandle*                 pTempModules)
{
    VkResult result = BuildShaderStageInfo(pDevice,
                                           pCreateInfo->stageCount,
                                           pCreateInfo->pStages,
                                           pCreateInfo->flags & VK_PIPELINE_CREATE_LIBRARY_BIT_KHR,
                                           [](const uint32_t inputIdx, const uint32_t stageIdx)
                                           {
                                               return stageIdx;
                                           },
                                           pShaderInfo->stages,
                                           pTempModules,
                                           pPipelineCache,
                                           pBinInfo->stageFeedback);

    if (result == VK_SUCCESS)
    {
        result = pDevice->GetCompiler(DefaultDeviceIndex)->ConvertGraphicsPipelineInfo(
            pDevice, pCreateInfo, pShaderInfo, pPipelineLayout, pBinInfo, pVbInfo, pInternalBufferInfo);
    }

    return result;
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
    Util::MetroHash128*                           pBaseHasher,
    Util::MetroHash128*                           pApiHasher)
{
    pBaseHasher->Update(desc.flags);
    pBaseHasher->Update(desc.topology);
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
    const uint32_t                           staticStateMask,
    Util::MetroHash128*                      pHasher)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.viewportCount);

    if ((staticStateMask & 1 << VK_DYNAMIC_STATE_VIEWPORT) && (desc.pViewports != nullptr))
    {
        for (uint32_t i = 0; i < desc.viewportCount; i++)
        {
            pHasher->Update(desc.pViewports[i]);
        }
    }

    pHasher->Update(desc.scissorCount);

    if ((staticStateMask & 1 << VK_DYNAMIC_STATE_SCISSOR) && (desc.pScissors != nullptr))
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
    Util::MetroHash128*                           pBaseHasher,
    Util::MetroHash128*                           pApiHasher)
{
    pBaseHasher->Update(desc.flags);
    pBaseHasher->Update(desc.depthClampEnable);
    pBaseHasher->Update(desc.polygonMode);
    pBaseHasher->Update(desc.cullMode);
    pBaseHasher->Update(desc.frontFace);
    pBaseHasher->Update(desc.depthBiasEnable);
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
        pBaseHasher->Update(desc.rasterizerDiscardEnable);
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
                pBaseHasher->Update(pExtInfo->sType);
                pBaseHasher->Update(pExtInfo->flags);
                pBaseHasher->Update(pExtInfo->rasterizationStream);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pNext);
                pBaseHasher->Update(pExtInfo->depthClipEnable);

                break;
            }
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineRasterizationLineStateCreateInfoEXT*>(pNext);
                pBaseHasher->Update(pExtInfo->lineRasterizationMode);
                pBaseHasher->Update(pExtInfo->stippledLineEnable);
                pBaseHasher->Update(pExtInfo->lineStippleFactor);
                pBaseHasher->Update(pExtInfo->lineStipplePattern);

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
    Util::MetroHash128*                         pBaseHasher,
    Util::MetroHash128*                         pApiHasher)
{
    pBaseHasher->Update(desc.flags);
    pBaseHasher->Update(desc.rasterizationSamples);
    pBaseHasher->Update(desc.sampleShadingEnable);
    pBaseHasher->Update(desc.minSampleShading);

    if (desc.pSampleMask != nullptr)
    {
        for (uint32_t i = 0; i < ceil(((float)desc.rasterizationSamples) / 32.0f); i++)
        {
            pApiHasher->Update(desc.pSampleMask[i]);
        }
    }

    pBaseHasher->Update(desc.alphaToCoverageEnable);
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
    Util::MetroHash128*                        pBaseHasher,
    Util::MetroHash128*                        pApiHasher)
{
    pBaseHasher->Update(desc.flags);
    pApiHasher->Update(desc.logicOpEnable);
    pApiHasher->Update(desc.logicOp);
    pBaseHasher->Update(desc.attachmentCount);

    for (uint32 i = 0; i < desc.attachmentCount; i++)
    {
        pBaseHasher->Update(desc.pAttachments[i]);
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
    Util::MetroHash128*                 pBaseHasher,
    Util::MetroHash128*                 pApiHasher)
{
    if (pCreateInfo->pVertexInputState != nullptr)
    {
        GenerateHashFromVertexInputStateCreateInfo(*pCreateInfo->pVertexInputState, pBaseHasher);
    }

    if (pCreateInfo->pInputAssemblyState != nullptr)
    {
        GenerateHashFromInputAssemblyStateCreateInfo(*pCreateInfo->pInputAssemblyState, pBaseHasher, pApiHasher);
    }
}

// =====================================================================================================================
void GraphicsPipelineCommon::GenerateHashForPreRasterizationShadersState(
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineObjectCreateInfo* pInfo,
    Util::MetroHash128*                     pBaseHasher,
    Util::MetroHash128*                     pApiHasher)
{
    for (uint32_t i = 0; i < pCreateInfo->stageCount; ++i)
    {
        if (pCreateInfo->pStages[i].stage & PrsShaderMask)
        {
            GenerateHashFromShaderStageCreateInfo(pCreateInfo->pStages[i], pBaseHasher);
        }
    }

    if (pCreateInfo->layout != VK_NULL_HANDLE)
    {
        pBaseHasher->Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());
    }

    if ((pInfo->immedInfo.rasterizerDiscardEnable == false) && (pCreateInfo->pViewportState != nullptr))
    {
        GenerateHashFromViewportStateCreateInfo(*pCreateInfo->pViewportState, pInfo->staticStateMask, pApiHasher);
    }

    if (pCreateInfo->pRasterizationState != nullptr)
    {
        bool rasterizerDiscardEnableDynamic = ((pInfo->staticStateMask &
                                                (1UL << static_cast<uint32_t>(DynamicStatesInternal::RasterizerDiscardEnableExt))) == 0);

        GenerateHashFromRasterizationStateCreateInfo(*pCreateInfo->pRasterizationState,
                                                     rasterizerDiscardEnableDynamic,
                                                     pBaseHasher,
                                                     pApiHasher);
    }

    if ((pInfo->activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        && (pCreateInfo->pTessellationState != nullptr))
    {
        GenerateHashFromTessellationStateCreateInfo(*pCreateInfo->pTessellationState, pBaseHasher);
    }

    if (pCreateInfo->renderPass != VK_NULL_HANDLE)
    {
        pBaseHasher->Update(RenderPass::ObjectFromHandle(pCreateInfo->renderPass)->GetHash());
    }

    pBaseHasher->Update(pCreateInfo->subpass);

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
    Util::MetroHash128*                     pBaseHasher,
    Util::MetroHash128*                     pApiHasher)
{
    for (uint32_t i = 0; i < pCreateInfo->stageCount; ++i)
    {
        if (pCreateInfo->pStages[i].stage & FgsShaderMask)
        {
            GenerateHashFromShaderStageCreateInfo(pCreateInfo->pStages[i], pBaseHasher);
        }
    }

    if (pCreateInfo->layout != VK_NULL_HANDLE)
    {
        pBaseHasher->Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());
    }

    if (pCreateInfo->pMultisampleState != nullptr)
    {
        GenerateHashFromMultisampleStateCreateInfo(*pCreateInfo->pMultisampleState, pBaseHasher, pApiHasher);
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
        GenerateHashFromDepthStencilStateCreateInfo(*pCreateInfo->pDepthStencilState, pApiHasher);
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
    Util::MetroHash128*                 pBaseHasher,
    Util::MetroHash128*                 pApiHasher)
{
    EXTRACT_VK_STRUCTURES_0(
        renderingCreateInfo,
        PipelineRenderingCreateInfoKHR,
        static_cast<const VkPipelineRenderingCreateInfoKHR*>(pCreateInfo->pNext),
        PIPELINE_RENDERING_CREATE_INFO_KHR);

    if (pPipelineRenderingCreateInfoKHR != nullptr)
    {
        pApiHasher->Update(pPipelineRenderingCreateInfoKHR->viewMask);
        pApiHasher->Update(pPipelineRenderingCreateInfoKHR->colorAttachmentCount);
        for (uint32_t i = 0; i < pPipelineRenderingCreateInfoKHR->colorAttachmentCount; ++i)
        {
            pApiHasher->Update(pPipelineRenderingCreateInfoKHR->pColorAttachmentFormats[i]);
        }
        pApiHasher->Update(pPipelineRenderingCreateInfoKHR->depthAttachmentFormat);
        pApiHasher->Update(pPipelineRenderingCreateInfoKHR->stencilAttachmentFormat);
    }

    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pCreateInfo->renderPass);

    if ((pCreateInfo->pColorBlendState != nullptr) &&
        (GetColorAttachmentCount(pRenderPass, pCreateInfo->subpass, pPipelineRenderingCreateInfoKHR) != 0))

    {
        GenerateHashFromColorBlendStateCreateInfo(*pCreateInfo->pColorBlendState, pBaseHasher, pApiHasher);
    }

    if (pCreateInfo->renderPass != VK_NULL_HANDLE)
    {
        pBaseHasher->Update(RenderPass::ObjectFromHandle(pCreateInfo->renderPass)->GetHash());
    }

    pBaseHasher->Update(pCreateInfo->subpass);
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
uint64_t GraphicsPipelineCommon::BuildApiHash(
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const GraphicsPipelineObjectCreateInfo* pInfo)
{
    Util::MetroHash::Hash baseHash;

    Util::MetroHash128 baseHasher;
    Util::MetroHash128 apiHasher;

    uint32_t dynamicStateFlags = GetDynamicStateFlags(
        pCreateInfo->pDynamicState
    );

    baseHasher.Update(pCreateInfo->flags);
    baseHasher.Update(dynamicStateFlags);

    const RenderPass* pRenderPass = RenderPass::ObjectFromHandle(pCreateInfo->renderPass);

    GenerateHashForVertexInputInterfaceState(pCreateInfo, &baseHasher, &apiHasher);

    GenerateHashForPreRasterizationShadersState(pCreateInfo, pInfo, &baseHasher, &apiHasher);

    const bool enableRasterization =
        (pInfo->immedInfo.rasterizerDiscardEnable == false) ||
        IsDynamicStateEnabled(dynamicStateFlags, DynamicStatesInternal::RasterizerDiscardEnableExt);

    if (enableRasterization)
    {
        GenerateHashForFragmentShaderState(pCreateInfo, &baseHasher, &apiHasher);

        GenerateHashForFragmentOutputInterfaceState(pCreateInfo, &baseHasher, &apiHasher);
    }

    if ((pCreateInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) && (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(GraphicsPipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);

    baseHasher.Finalize(reinterpret_cast<uint8_t*>(&baseHash));

    uint64_t              apiHash;
    Util::MetroHash::Hash apiHashFull;
    apiHasher.Update(baseHash);
    apiHasher.Finalize(reinterpret_cast<uint8_t*>(&apiHashFull));
    apiHash = Util::MetroHash::Compact64(&apiHashFull);

    return apiHash;
}

}
