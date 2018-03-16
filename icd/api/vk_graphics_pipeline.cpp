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

#include "include/stencil_ops_combiner.h"
#include "include/vk_conv.h"
#include "include/vk_device.h"
#include "include/vk_graphics_pipeline.h"
#include "include/vk_instance.h"
#include "include/vk_memory.h"
#include "include/vk_pipeline_cache.h"
#include "include/vk_pipeline_layout.h"
#include "include/vk_object.h"
#include "include/vk_render_pass.h"
#include "include/vk_shader.h"
#include "include/vk_cmdbuffer.h"

#include "palAutoBuffer.h"
#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palPipeline.h"
#include "palShader.h"
#include "palInlineFuncs.h"

#include <float.h>
#include <math.h>

using namespace Util;

namespace vk
{

// =====================================================================================================================
// Returns true if the given blend factor is a dual source blend factor
bool IsDualSourceBlend(Pal::Blend blend)
{
    switch (blend)
    {
    case Pal::Blend::Src1Color:
    case Pal::Blend::OneMinusSrc1Color:
    case Pal::Blend::Src1Alpha:
    case Pal::Blend::OneMinusSrc1Alpha:
        return true;
    default:
        return false;
    }
}

// =====================================================================================================================
// Returns true if src alpha is used in blending
bool IsSrcAlphaUsedInBlend(
    VkBlendFactor blend)
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
// Parses input pipeline rasterization create info state.
void GraphicsPipeline::BuildRasterizationState(
    Device*                                       pDevice,
    const VkPipelineRasterizationStateCreateInfo* pIn,
    CreateInfo*                                   pInfo,
    const bool                                    dynamicStateFlags[])
{
    union
    {
        const VkStructHeader*                                       pHeader;
        const VkPipelineRasterizationStateCreateInfo*               pRs;
        const VkPipelineRasterizationStateRasterizationOrderAMD*    pRsOrder;
    };

    // By default rasterization is disabled, unless rasterization creation info is present

    const VkPhysicalDeviceLimits& limits = pDevice->VkPhysicalDevice()->GetLimits();

    // Enable perpendicular end caps if we report strictLines semantics
    pInfo->pipeline.rsState.perpLineEndCapsEnable = (limits.strictLines == VK_TRUE);

    for (pRs = pIn; pHeader != nullptr; pHeader = pHeader->pNext)
    {
        switch (pHeader->sType)
        {
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO:
            {
                pInfo->pipeline.rsState.depthClampDisable = (pRs->depthClampEnable == VK_FALSE);
                // When depth clamping is enabled, depth clipping should be disabled, and vice versa
                pInfo->immedInfo.triangleRasterState.fillMode  = VkToPalFillMode(pRs->polygonMode);
                pInfo->immedInfo.triangleRasterState.cullMode  = VkToPalCullMode(pRs->cullMode);
                pInfo->immedInfo.triangleRasterState.frontFace = VkToPalFaceOrientation(pRs->frontFace);
                pInfo->immedInfo.triangleRasterState.flags.depthBiasEnable = pRs->depthBiasEnable;

                pInfo->immedInfo.depthBiasParams.depthBias = pRs->depthBiasConstantFactor;
                pInfo->immedInfo.depthBiasParams.depthBiasClamp = pRs->depthBiasClamp;
                pInfo->immedInfo.depthBiasParams.slopeScaledDepthBias = pRs->depthBiasSlopeFactor;

                if (pRs->depthBiasEnable && (dynamicStateFlags[VK_DYNAMIC_STATE_DEPTH_BIAS] == false))
                {
                    pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_DEPTH_BIAS;
                }

                // point size must be set via gl_PointSize, otherwise it must be 1.0f.
                constexpr float DefaultPointSize = 1.0f;

                pInfo->immedInfo.pointLineRasterParams.lineWidth    = pRs->lineWidth;
                pInfo->immedInfo.pointLineRasterParams.pointSize    = DefaultPointSize;
                pInfo->immedInfo.pointLineRasterParams.pointSizeMin = limits.pointSizeRange[0];
                pInfo->immedInfo.pointLineRasterParams.pointSizeMax = limits.pointSizeRange[1];

                if (dynamicStateFlags[VK_DYNAMIC_STATE_LINE_WIDTH] == false)
                {
                    pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_LINE_WIDTH;
                }
            }
            break;

        default:
            // Handle  extension specific structures
            // (a separate switch-case is used to allow the main switch-case to be optimized into a lookup table)
            switch (static_cast<int32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
                {
                    // VK_AMD_rasterization_order must be enabled
                    VK_ASSERT(pDevice->IsExtensionEnabled(DeviceExtensions::AMD_RASTERIZATION_ORDER));

                    pInfo->pipeline.rsState.outOfOrderPrimsEnable =
                        VkToPalRasterizationOrder(pRsOrder->rasterizationOrder);
                }
                break;

            default:
                // Skip any unknown extension structures
                break;
            }
            break;
        }
    }
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
void GraphicsPipeline::ConvertGraphicsPipelineInfo(
    Device*                             pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    CreateInfo*                         pInfo)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    VkFormat cbFormat[Pal::MaxColorTargets] = {};

    // Fill in necessary non-zero defaults in case some information is missing
    pInfo->msaa.coverageSamples         = 1;
    pInfo->msaa.pixelShaderSamples      = 1;
    pInfo->msaa.depthStencilSamples     = 1;
    pInfo->msaa.shaderExportMaskSamples = 1;
    pInfo->msaa.sampleClusters          = 1;
    pInfo->msaa.alphaToCoverageSamples  = 1;
    pInfo->msaa.occlusionQuerySamples   = 1;
    pInfo->msaa.sampleMask              = 1;
    pInfo->sampleCoverage               = 1;

    EXTRACT_VK_STRUCTURES_0(
        gfxPipeline,
        GraphicsPipelineCreateInfo,
        pIn,
        GRAPHICS_PIPELINE_CREATE_INFO)

    const RenderPass* pRenderPass = nullptr;

    // Set the states which are allowed to call CmdSetxxx outside of the PSO
    bool dynamicStateFlags[uint32_t(DynamicStatesInternal::DynamicStatesInternalCount)];

    memset(dynamicStateFlags, 0, sizeof(dynamicStateFlags));

    if (pGraphicsPipelineCreateInfo != nullptr)
    {
        for (uint32_t i = 0; i < pGraphicsPipelineCreateInfo->stageCount; ++i)
        {
            pInfo->activeStages = static_cast<VkShaderStageFlagBits>(
                pInfo->activeStages | pGraphicsPipelineCreateInfo->pStages[i].stage);
        }
        VK_IGNORE(pGraphicsPipelineCreateInfo->flags & VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT);

        pRenderPass = RenderPass::ObjectFromHandle(pGraphicsPipelineCreateInfo->renderPass);

        if (pGraphicsPipelineCreateInfo->layout != VK_NULL_HANDLE)
        {
            pInfo->pLayout = PipelineLayout::ObjectFromHandle(pGraphicsPipelineCreateInfo->layout);
        }

        const VkPipelineInputAssemblyStateCreateInfo* pIa = pGraphicsPipelineCreateInfo->pInputAssemblyState;

        // According to the spec this should never be null
        VK_ASSERT(pIa != nullptr);

        pInfo->immedInfo.inputAssemblyState.primitiveRestartEnable = (pIa->primitiveRestartEnable != VK_FALSE);
        pInfo->immedInfo.inputAssemblyState.primitiveRestartIndex  = (pIa->primitiveRestartEnable != VK_FALSE)
                                                                     ? 0xFFFFFFFF : 0;
        pInfo->immedInfo.inputAssemblyState.topology = VkToPalPrimitiveTopology(pIa->topology);

        VkToPalPrimitiveTypeAdjacency(
            pIa->topology,
            &pInfo->pipeline.iaState.topologyInfo.primitiveType,
            &pInfo->pipeline.iaState.topologyInfo.adjacency);

        EXTRACT_VK_STRUCTURES_0(
            Tess,
            PipelineTessellationStateCreateInfo,
            pGraphicsPipelineCreateInfo->pTessellationState,
            PIPELINE_TESSELLATION_STATE_CREATE_INFO)

            if (pPipelineTessellationStateCreateInfo != nullptr)
            {
                pInfo->pipeline.iaState.topologyInfo.patchControlPoints = pPipelineTessellationStateCreateInfo->patchControlPoints;
            }

        pInfo->immedInfo.staticStateMask = 0;

        const VkPipelineDynamicStateCreateInfo* pDy = pGraphicsPipelineCreateInfo->pDynamicState;

        if (pDy != nullptr)
        {
            for (uint32_t i = 0; i < pDy->dynamicStateCount; ++i)
            {
                if (pDy->pDynamicStates[i] < VK_DYNAMIC_STATE_RANGE_SIZE)
                {
                    dynamicStateFlags[pDy->pDynamicStates[i]] = true;
                }
                else
                {
                    switch (static_cast<uint32_t>(pDy->pDynamicStates[i]))
                    {
                    case VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT)] = true;
                        break;

                    default:
                        // skip unknown dynamic state
                        break;
                    }
                }
            }
        }

        const VkPipelineViewportStateCreateInfo* pVp = pGraphicsPipelineCreateInfo->pViewportState;

        if (pVp != nullptr)
        {
            // From the spec, "scissorCount is the number of scissors and must match the number of viewports."
            VK_ASSERT(pVp->viewportCount <= Pal::MaxViewports);
            VK_ASSERT(pVp->scissorCount <= Pal::MaxViewports);
            VK_ASSERT(pVp->scissorCount == pVp->viewportCount);

            pInfo->immedInfo.viewportParams.count     = pVp->viewportCount;
            pInfo->immedInfo.scissorRectParams.count  = pVp->scissorCount;

            if (dynamicStateFlags[VK_DYNAMIC_STATE_VIEWPORT] == false)
            {
                VK_ASSERT(pVp->pViewports != nullptr);

                const bool khrMaintenance1 =
                    ((pDevice->VkPhysicalDevice()->GetEnabledAPIVersion() >= VK_MAKE_VERSION(1, 1, 0)) ||
                     pDevice->IsExtensionEnabled(DeviceExtensions::KHR_MAINTENANCE1));

                for (uint32_t i = 0; i < pVp->viewportCount; ++i)
                {
                    VkToPalViewport(
                        pVp->pViewports[i],
                        i,
                        khrMaintenance1,
                        &pInfo->immedInfo.viewportParams);
                }

                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_VIEWPORT;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_SCISSOR] == false)
            {
                VK_ASSERT(pVp->pScissors != nullptr);

                for (uint32_t i = 0; i < pVp->scissorCount; ++i)
                {
                    VkToPalScissorRect(pVp->pScissors[i], i, &pInfo->immedInfo.scissorRectParams);
                }

                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_SCISSOR;
            }
        }

        BuildRasterizationState(pDevice,
                                pGraphicsPipelineCreateInfo->pRasterizationState,
                                pInfo,
                                dynamicStateFlags);

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

        bool multisampleEnable = false;
        uint32_t rasterizationSampleCount = 0;

        const VkPipelineMultisampleStateCreateInfo* pMs = pGraphicsPipelineCreateInfo->pMultisampleState;

        if (pMs != nullptr)
        {
            multisampleEnable = (pMs->rasterizationSamples != 1);

            if (multisampleEnable)
            {
                VK_ASSERT(pRenderPass != nullptr);

                rasterizationSampleCount            = pMs->rasterizationSamples;
                uint32_t subpassCoverageSampleCount = pRenderPass->GetSubpassMaxSampleCount(pGraphicsPipelineCreateInfo->subpass);
                uint32_t subpassColorSampleCount    = pRenderPass->GetSubpassColorSampleCount(pGraphicsPipelineCreateInfo->subpass);
                uint32_t subpassDepthSampleCount    = pRenderPass->GetSubpassDepthSampleCount(pGraphicsPipelineCreateInfo->subpass);

                // subpassCoverageSampleCount would be equal to zero if there are zero attachments.
                subpassCoverageSampleCount = subpassCoverageSampleCount == 0 ? rasterizationSampleCount : subpassCoverageSampleCount;

                // In case we are rendering to color only, we make sure to set the DepthSampleCount to CoverageSampleCount.
                // CoverageSampleCount is really the ColorSampleCount in this case. This makes sure we have a consistent
                // sample count and that we get correct MSAA behavior.
                // Similar thing for when we are rendering to depth only. The expectation in that case is that all
                // sample counts should match.
                // This shouldn't interfere with EQAA. For EQAA, if ColorSampleCount is not equal to DepthSampleCount
                // and they are both greater than one, then we do not force them to match.
                subpassColorSampleCount = subpassColorSampleCount == 0 ? subpassCoverageSampleCount : subpassColorSampleCount;
                subpassDepthSampleCount = subpassDepthSampleCount == 0 ? subpassCoverageSampleCount : subpassDepthSampleCount;

                VK_ASSERT(rasterizationSampleCount == subpassCoverageSampleCount);

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

                /// Sample Locations
                EXTRACT_VK_STRUCTURES_1(
                    SampleLocations,
                    PipelineMultisampleStateCreateInfo,
                    PipelineSampleLocationsStateCreateInfoEXT,
                    pMs,
                    PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT)

                    bool customSampleLocations = false;

                if (pPipelineSampleLocationsStateCreateInfoEXT != nullptr)
                {
                    customSampleLocations = pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsEnable == VK_TRUE
                        ? true
                        : customSampleLocations;
                }

                if (customSampleLocations &&
                    (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT)] == false))
                {
                    // We store the custom sample locations if custom sample locations are enabled and the
                    // sample locations state is static.
                    pInfo->immedInfo.samplePattern.sampleCount =
                        (uint32_t)pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationsPerPixel;

                    ConvertToPalMsaaQuadSamplePattern(
                        &pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo,
                        &pInfo->immedInfo.samplePattern.locations);

                    VK_ASSERT(pInfo->immedInfo.samplePattern.sampleCount == rasterizationSampleCount);

                    pInfo->immedInfo.staticStateMask |=
                        (1 << static_cast<uint32_t>(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT));
                }
                else if (customSampleLocations == false)
                {
                    // We store the standard sample locations if custom sample locations are not enabled.
                    pInfo->immedInfo.samplePattern.sampleCount = rasterizationSampleCount;
                    pInfo->immedInfo.samplePattern.locations =
                        *Device::GetDefaultQuadSamplePattern(rasterizationSampleCount);

                    pInfo->immedInfo.staticStateMask |=
                        1 << static_cast<uint32_t>(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT);
                }
            }

            pInfo->pipeline.cbState.alphaToCoverageEnable     = (pMs->alphaToCoverageEnable == VK_TRUE);
        }

        const VkPipelineColorBlendStateCreateInfo* pCb = pGraphicsPipelineCreateInfo->pColorBlendState;

        bool blendingEnabled = false;
        bool dualSourceBlend = false;

        if (pCb == nullptr)
        {
            pInfo->pipeline.cbState.logicOp = Pal::LogicOp::Copy;
        }
        else
        {
            pInfo->pipeline.cbState.logicOp = (pCb->logicOpEnable) ? VkToPalLogicOp(pCb->logicOp) :
                Pal::LogicOp::Copy;

            const uint32_t numColorTargets = Min(pCb->attachmentCount, Pal::MaxColorTargets);

            for (uint32_t i = 0; i < numColorTargets; ++i)
            {
                const VkPipelineColorBlendAttachmentState& src = pCb->pAttachments[i];

                auto pCbDst     = &pInfo->pipeline.cbState.target[i];
                auto pBlendDst  = &pInfo->blend.targets[i];

                if (pRenderPass)
                {
                    cbFormat[i] = pRenderPass->GetColorAttachmentFormat(pGraphicsPipelineCreateInfo->subpass, i);
                    pCbDst->swizzledFormat = VkToPalFormat(cbFormat[i]);
                }

                // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
                // want to write to any attachment for that output (VK_ATTACHMENT_UNUSED).  Under such cases,
                // disable shader writes through that target.
                if (pCbDst->swizzledFormat.format != Pal::ChNumFormat::Undefined)
                {
                    pCbDst->channelWriteMask         = src.colorWriteMask;
                    blendingEnabled |= (src.blendEnable == VK_TRUE);
                }

                pBlendDst->blendEnable    = (src.blendEnable == VK_TRUE);
                pBlendDst->srcBlendColor  = VkToPalBlend(src.srcColorBlendFactor);
                pBlendDst->dstBlendColor  = VkToPalBlend(src.dstColorBlendFactor);
                pBlendDst->blendFuncColor = VkToPalBlendFunc(src.colorBlendOp);
                pBlendDst->srcBlendAlpha  = VkToPalBlend(src.srcAlphaBlendFactor);
                pBlendDst->dstBlendAlpha  = VkToPalBlend(src.dstAlphaBlendFactor);
                pBlendDst->blendFuncAlpha = VkToPalBlendFunc(src.alphaBlendOp);

                dualSourceBlend |= IsDualSourceBlend(pBlendDst->srcBlendColor);
                dualSourceBlend |= IsDualSourceBlend(pBlendDst->dstBlendColor);
                dualSourceBlend |= IsDualSourceBlend(pBlendDst->srcBlendAlpha);
                dualSourceBlend |= IsDualSourceBlend(pBlendDst->dstBlendAlpha);
            }
        }

        pInfo->pipeline.cbState.dualSourceBlendEnable     = dualSourceBlend;

        if (blendingEnabled == true && dynamicStateFlags[VK_DYNAMIC_STATE_BLEND_CONSTANTS] == false)
        {
            static_assert(sizeof(pInfo->immedInfo.blendConstParams) == sizeof(pCb->blendConstants),
                "Blend constant structure size mismatch!");

            memcpy(&pInfo->immedInfo.blendConstParams, pCb->blendConstants, sizeof(pCb->blendConstants));

            pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_BLEND_CONSTANTS;
        }

        VkFormat dbFormat = { };
        if (pRenderPass != nullptr)
        {
            dbFormat = pRenderPass->GetDepthStencilAttachmentFormat(pGraphicsPipelineCreateInfo->subpass);
        }

        // If the sub pass attachment format is UNDEFINED, then it means that that subpass does not
        // want to write any depth-stencil data (VK_ATTACHMENT_UNUSED).  Under such cases, I think we have to
        // disable depth testing as well as depth writes.
        const VkPipelineDepthStencilStateCreateInfo* pDs = pGraphicsPipelineCreateInfo->pDepthStencilState;

        if ((dbFormat != VK_FORMAT_UNDEFINED) && (pDs != nullptr))
        {
            pInfo->ds.stencilEnable     = (pDs->stencilTestEnable == VK_TRUE);
            pInfo->ds.depthEnable       = (pDs->depthTestEnable == VK_TRUE);
            pInfo->ds.depthWriteEnable  = (pDs->depthWriteEnable == VK_TRUE);
            pInfo->ds.depthFunc         = VkToPalCompareFunc(pDs->depthCompareOp);
            pInfo->ds.depthBoundsEnable = (pDs->depthBoundsTestEnable == VK_TRUE);

            if (pInfo->ds.depthBoundsEnable && dynamicStateFlags[VK_DYNAMIC_STATE_DEPTH_BOUNDS] == false)
            {
                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_DEPTH_BOUNDS;
            }

            // According to Graham, we should program the stencil state at PSO bind time,
            // regardless of whether this PSO enables\disables Stencil. This allows a second PSO
            // to inherit the first PSO's settings
            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK] == false)
            {
                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_WRITE_MASK] == false)
            {
                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_REFERENCE] == false)
            {
                pInfo->immedInfo.staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_REFERENCE;
            }
        }
        else
        {
            pInfo->ds.depthEnable       = false;
            pInfo->ds.depthWriteEnable  = false;
            pInfo->ds.depthFunc         = Pal::CompareFunc::Always;
            pInfo->ds.depthBoundsEnable = false;
            pInfo->ds.stencilEnable     = false;
        }

        constexpr uint8_t DefaultStencilOpValue = 1;

        if (pDs != nullptr)
        {
            pInfo->ds.front.stencilFailOp      = VkToPalStencilOp(pDs->front.failOp);
            pInfo->ds.front.stencilPassOp      = VkToPalStencilOp(pDs->front.passOp);
            pInfo->ds.front.stencilDepthFailOp = VkToPalStencilOp(pDs->front.depthFailOp);
            pInfo->ds.front.stencilFunc        = VkToPalCompareFunc(pDs->front.compareOp);
            pInfo->ds.back.stencilFailOp       = VkToPalStencilOp(pDs->back.failOp);
            pInfo->ds.back.stencilPassOp       = VkToPalStencilOp(pDs->back.passOp);
            pInfo->ds.back.stencilDepthFailOp  = VkToPalStencilOp(pDs->back.depthFailOp);
            pInfo->ds.back.stencilFunc         = VkToPalCompareFunc(pDs->back.compareOp);

            pInfo->immedInfo.stencilRefMasks.frontRef       = static_cast<uint8_t>(pDs->front.reference);
            pInfo->immedInfo.stencilRefMasks.frontReadMask  = static_cast<uint8_t>(pDs->front.compareMask);
            pInfo->immedInfo.stencilRefMasks.frontWriteMask = static_cast<uint8_t>(pDs->front.writeMask);
            pInfo->immedInfo.stencilRefMasks.backRef        = static_cast<uint8_t>(pDs->back.reference);
            pInfo->immedInfo.stencilRefMasks.backReadMask   = static_cast<uint8_t>(pDs->back.compareMask);
            pInfo->immedInfo.stencilRefMasks.backWriteMask  = static_cast<uint8_t>(pDs->back.writeMask);

            pInfo->immedInfo.depthBoundParams.min = pDs->minDepthBounds;
            pInfo->immedInfo.depthBoundParams.max = pDs->maxDepthBounds;
        }

        pInfo->immedInfo.stencilRefMasks.frontOpValue   = DefaultStencilOpValue;
        pInfo->immedInfo.stencilRefMasks.backOpValue    = DefaultStencilOpValue;

        pInfo->pipeline.viewInstancingDesc = Pal::ViewInstancingDescriptor { };

        if (pRenderPass->IsMultiviewEnabled())
        {
            pInfo->pipeline.viewInstancingDesc.viewInstanceCount = Pal::MaxViewInstanceCount;
            pInfo->pipeline.viewInstancingDesc.enableMasking     = true;

            for (uint32 viewIndex = 0; viewIndex < Pal::MaxViewInstanceCount; ++viewIndex)
            {
                pInfo->pipeline.viewInstancingDesc.viewId[viewIndex] = viewIndex;
            }
        }
    }

}

// =====================================================================================================================
// Create a graphics pipeline object.
VkResult GraphicsPipeline::Create(
    Device*                                 pDevice,
    PipelineCache*                          pPipelineCache,
    const VkGraphicsPipelineCreateInfo*     pCreateInfo,
    const VkAllocationCallbacks*            pAllocator,
    VkPipeline*                             pPipeline)
{
    // Parse the create info and build patched AMDIL shaders
    CreateInfo    createInfo                         = {};
    VbBindingInfo vbInfo                             = {};
    PipelineCompiler::GraphicsPipelineCreateInfo binaryCreateInfo = {};
    size_t        pipelineBinarySizes[MaxPalDevices] = {};
    const void*   pPipelineBinaries[MaxPalDevices]   = {};
    Pal::Result   palResult                          = Pal::Result::Success;
    PipelineCompiler*     pDefaultCompiler = pDevice->GetCompiler();

    VkResult result = pDefaultCompiler->ConvertGraphicsPipelineInfo(pDevice, pCreateInfo, &binaryCreateInfo, &vbInfo);
    const uint32_t numPalDevices = pDevice->NumPalDevices();
    for (uint32_t i = 0; (result == VK_SUCCESS) && (i < numPalDevices); ++i)
    {
        result = pDevice->GetCompiler(i)->CreateGraphicsPipelineBinary(
        pDevice,
        i,
        pPipelineCache,
        &binaryCreateInfo,
        &pipelineBinarySizes[i],
        &pPipelineBinaries[i]);
    }

    if (result == VK_SUCCESS)
    {
        ConvertGraphicsPipelineInfo(pDevice, pCreateInfo, &createInfo);

    }

    RenderStateCache* pRSCache = pDevice->GetRenderStateCache();

    // Get the pipeline size from PAL and allocate memory.
    const size_t palSize = pDevice->PalDevice()->GetGraphicsPipelineSize(createInfo.pipeline, &palResult);
    VK_ASSERT(palResult == Pal::Result::Success);

    void* pSystemMem = nullptr;

    if (result == VK_SUCCESS)
    {
        pSystemMem = pAllocator->pfnAllocation(
            pAllocator->pUserData,
            sizeof(GraphicsPipeline) + (palSize * numPalDevices),
            VK_DEFAULT_MEM_ALIGN,
            VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);

        if (pSystemMem == nullptr)
        {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    // Create the PAL pipeline object.
    Pal::IPipeline*          pPalPipeline[MaxPalDevices]     = {};
    Pal::IMsaaState*         pPalMsaa[MaxPalDevices]         = {};
    Pal::IColorBlendState*   pPalColorBlend[MaxPalDevices]   = {};
    Pal::IDepthStencilState* pPalDepthStencil[MaxPalDevices] = {};

    if (result == VK_SUCCESS)
    {
        size_t palOffset = sizeof(GraphicsPipeline);

        for (uint32_t deviceIdx = 0; deviceIdx < numPalDevices; deviceIdx++)
        {
            Pal::IDevice* pPalDevice = pDevice->PalDevice(deviceIdx);

            if (palResult == Pal::Result::Success)
            {
                // If pPipelineBinaries[DefaultDeviceIndex] is sufficient for all devices, the other pipeline binaries
                // won't be created.  Otherwise, like if gl_DeviceIndex is used, they will be.
                if (pPipelineBinaries[deviceIdx] != nullptr)
                {
                    createInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[deviceIdx];
                    createInfo.pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
                }

                palResult = pPalDevice->CreateGraphicsPipeline(
                    createInfo.pipeline,
                    Util::VoidPtrInc(pSystemMem, palOffset),
                    &pPalPipeline[deviceIdx]);

                VK_ASSERT(palSize == pPalDevice->GetGraphicsPipelineSize(createInfo.pipeline, nullptr));
                palOffset += palSize;
            }

            // Create the PAL MSAA state object
            if (palResult == Pal::Result::Success)
            {
                const auto& pMs = pCreateInfo->pMultisampleState;

                // Force full sample shading if the app didn't enable it, but the shader wants
                // per-sample shading by the use of SampleId or similar features.
                if ((pMs != nullptr) && (pMs->sampleShadingEnable == false))
                {
                    const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                    if (info.ps.flags.perSampleShading == 1)
                    {
                        createInfo.msaa.pixelShaderSamples = createInfo.msaa.coverageSamples;
                    }
                }

                palResult = pRSCache->CreateMsaaState(
                    createInfo.msaa,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalMsaa);
            }

            // Create the PAL color blend state object
            if (palResult == Pal::Result::Success)
            {
                palResult = pRSCache->CreateColorBlendState(
                    createInfo.blend,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalColorBlend);
            }

            // Create the PAL depth stencil state object
            if (palResult == Pal::Result::Success)
            {
                palResult = pRSCache->CreateDepthStencilState(
                    createInfo.ds,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalDepthStencil);
            }
        }

        result = PalToVkResult(palResult);
    }

    PipelineBinaryInfo* pBinaryInfo = nullptr;

    if (pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) && (result == VK_SUCCESS))
    {
        pBinaryInfo = PipelineBinaryInfo::Create(
            pipelineBinarySizes[DefaultDeviceIndex],
            pPipelineBinaries[DefaultDeviceIndex],
            pAllocator);
    }

    const bool viewIndexFromDeviceIndex = Util::TestAnyFlagSet(
        pCreateInfo->flags,
        VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT_KHX);

    // On success, wrap it up in a Vulkan object.
    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) GraphicsPipeline(
            pDevice,
            pPalPipeline,
            createInfo.pLayout,
            createInfo.immedInfo,
            vbInfo,
            pPalMsaa,
            pPalColorBlend,
            pPalDepthStencil,
            createInfo.sampleCoverage,
            viewIndexFromDeviceIndex,
            pBinaryInfo);

        *pPipeline = GraphicsPipeline::HandleFromVoidPointer(pSystemMem);
    }

    // Free the created pipeline binaries now that the PAL Pipelines/PipelineBinaryInfo have read them.
    for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
    {
        if (pPipelineBinaries[deviceIdx] != nullptr)
        {
            pDevice->GetCompiler(deviceIdx)->FreeGraphicsPipelineBinary(
                &binaryCreateInfo, pPipelineBinaries[deviceIdx], pipelineBinarySizes[deviceIdx]);
        }
    }
    pDefaultCompiler->FreeGraphicsPipelineCreateInfo(&binaryCreateInfo);

    if (result != VK_SUCCESS)
    {
        pRSCache->DestroyMsaaState(pPalMsaa, pAllocator);
        pRSCache->DestroyColorBlendState(pPalColorBlend, pAllocator);
        pRSCache->DestroyDepthStencilState(pPalDepthStencil, pAllocator);

        // Something went wrong with creating the PAL object. Free memory and return error.
        for (uint32_t deviceIdx = 0; deviceIdx < pDevice->NumPalDevices(); deviceIdx++)
        {
            if (pPalPipeline[deviceIdx] != nullptr)
            {
                pPalPipeline[deviceIdx]->Destroy();
            }
        }

        if (pBinaryInfo != nullptr)
        {
            pBinaryInfo->Destroy(pAllocator);
        }

        pAllocator->pfnFree(pAllocator->pUserData, pSystemMem);
    }

    return result;
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
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
    PipelineBinaryInfo*                    pBinary)
    :
    Pipeline(pDevice, pPalPipeline, pLayout, pBinary),
    m_info(immedInfo),
    m_vbInfo(vbInfo),
    m_coverageSamples(coverageSamples),
    m_flags()
{
    m_flags.viewIndexFromDeviceIndex = viewIndexFromDeviceIndex;

    memcpy(m_pPalMsaa,         pPalMsaa,         sizeof(pPalMsaa[0])         * pDevice->NumPalDevices());
    memcpy(m_pPalColorBlend,   pPalColorBlend,   sizeof(pPalColorBlend[0])   * pDevice->NumPalDevices());
    memcpy(m_pPalDepthStencil, pPalDepthStencil, sizeof(pPalDepthStencil[0]) * pDevice->NumPalDevices());

    CreateStaticState();
}

// =====================================================================================================================
// Creates instances of static pipeline state.  Much of this information can be cached at the device-level to help speed
// up pipeline-bind operations.
void GraphicsPipeline::CreateStaticState()
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();
    auto* pStaticTokens      = &m_info.staticTokens;

    pStaticTokens->inputAssemblyState         = pCache->CreateInputAssemblyState(m_info.inputAssemblyState);
    pStaticTokens->triangleRasterState        = pCache->CreateTriangleRasterState(m_info.triangleRasterState);
    pStaticTokens->pointLineRasterState       = DynamicRenderStateToken;
    pStaticTokens->depthBias                  = DynamicRenderStateToken;
    pStaticTokens->blendConst                 = DynamicRenderStateToken;
    pStaticTokens->depthBounds                = DynamicRenderStateToken;
    pStaticTokens->viewport                   = DynamicRenderStateToken;
    pStaticTokens->scissorRect                = DynamicRenderStateToken;
    pStaticTokens->samplePattern              = DynamicRenderStateToken;
    pStaticTokens->waveLimits                 = DynamicRenderStateToken;

    if (PipelineSetsState(DynamicStatesInternal::LINE_WIDTH))
    {
        pStaticTokens->pointLineRasterState = pCache->CreatePointLineRasterState(
            m_info.pointLineRasterParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::DEPTH_BIAS))
    {
        pStaticTokens->depthBias = pCache->CreateDepthBias(m_info.depthBiasParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::BLEND_CONSTANTS))
    {
        pStaticTokens->blendConst = pCache->CreateBlendConst(m_info.blendConstParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::DEPTH_BOUNDS))
    {
        pStaticTokens->depthBounds = pCache->CreateDepthBounds(m_info.depthBoundParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::VIEWPORT))
    {
        pStaticTokens->viewport = pCache->CreateViewport(m_info.viewportParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::SCISSOR))
    {
        pStaticTokens->scissorRect = pCache->CreateScissorRect(m_info.scissorRectParams);
    }

    if (PipelineSetsState(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT))
    {
        pStaticTokens->samplePattern = pCache->CreateSamplePattern(m_info.samplePattern);
    }

}

// =====================================================================================================================
// Destroys static pipeline state.
void GraphicsPipeline::DestroyStaticState(
    const VkAllocationCallbacks* pAllocator)
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();

    pCache->DestroyMsaaState(m_pPalMsaa, pAllocator);
    pCache->DestroyColorBlendState(m_pPalColorBlend, pAllocator);
    pCache->DestroyDepthStencilState(m_pPalDepthStencil, pAllocator);

    pCache->DestroyInputAssemblyState(m_info.inputAssemblyState,
                                      m_info.staticTokens.inputAssemblyState);

    pCache->DestroyTriangleRasterState(m_info.triangleRasterState,
                                       m_info.staticTokens.triangleRasterState);

    pCache->DestroyPointLineRasterState(m_info.pointLineRasterParams,
                                        m_info.staticTokens.pointLineRasterState);

    pCache->DestroyDepthBias(m_info.depthBiasParams,
                             m_info.staticTokens.depthBias);

    pCache->DestroyBlendConst(m_info.blendConstParams,
                              m_info.staticTokens.blendConst);

    pCache->DestroyDepthBounds(m_info.depthBoundParams,
                               m_info.staticTokens.depthBounds);

    pCache->DestroyViewport(m_info.viewportParams,
                            m_info.staticTokens.viewport);

    pCache->DestroyScissorRect(m_info.scissorRectParams,
                               m_info.staticTokens.scissorRect);

    pCache->DestroySamplePattern(m_info.samplePattern,
                                 m_info.staticTokens.samplePattern);
}

// =====================================================================================================================
VkResult GraphicsPipeline::Destroy(
    Device*                      pDevice,
    const VkAllocationCallbacks* pAllocator)
{
    DestroyStaticState(pAllocator);

    return Pipeline::Destroy(pDevice, pAllocator);
}

// =====================================================================================================================
GraphicsPipeline::~GraphicsPipeline()
{

}

// =====================================================================================================================
// Binds this graphics pipeline's state to the given command buffer (using waveLimits created from the pipeline)
void GraphicsPipeline::BindToCmdBuffer(
    CmdBuffer*                             pCmdBuffer,
    CmdBufferRenderState*                  pRenderState,
    StencilOpsCombiner*                    pStencilCombiner) const
{
    BindToCmdBuffer(pCmdBuffer, pRenderState, pStencilCombiner, m_info.graphicsWaveLimitParams);
}

// =====================================================================================================================
// Binds this graphics pipeline's state to the given command buffer (with passed in wavelimits)
void GraphicsPipeline::BindToCmdBuffer(
    CmdBuffer*                             pCmdBuffer,
    CmdBufferRenderState*                  pRenderState,
    StencilOpsCombiner*                    pStencilCombiner,
    const Pal::DynamicGraphicsShaderInfos& graphicsShaderInfos) const
{
    // If the viewport/scissor counts changed, we need to resend the current viewport/scissor state to PAL
    bool viewportCountDirty = (pRenderState->allGpuState.viewport.count != m_info.viewportParams.count);
    bool scissorCountDirty  = (pRenderState->allGpuState.scissor.count  != m_info.scissorRectParams.count);

    // Update current viewport/scissor count
    pRenderState->allGpuState.viewport.count = m_info.viewportParams.count;
    pRenderState->allGpuState.scissor.count  = m_info.scissorRectParams.count;

    // Get this pipeline's static tokens
    const auto& newTokens = m_info.staticTokens;

    // Get the old static tokens.  Copy these by value because in MGPU cases we update the new token state in a loop.
    const auto oldTokens = pRenderState->allGpuState.staticTokens;

    // Program static pipeline state.

    // This code will attempt to skip programming state state based on redundant value checks.  These checks are often
    // represented as token compares, where the tokens are two perfect hashes of previously compiled pipelines' static
    // parameter values.
    if (PipelineSetsState(DynamicStatesInternal::VIEWPORT) &&
        CmdBuffer::IsStaticStateDifferent(oldTokens.viewports, newTokens.viewport))
    {
        pCmdBuffer->SetAllViewports(m_info.viewportParams, newTokens.viewport);
        viewportCountDirty = false;
    }

    if (PipelineSetsState(DynamicStatesInternal::SCISSOR) &&
        CmdBuffer::IsStaticStateDifferent(oldTokens.scissorRect, newTokens.scissorRect))
    {
        pCmdBuffer->SetAllScissors(m_info.scissorRectParams, newTokens.scissorRect);
        scissorCountDirty = false;
    }

    utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
    while (deviceGroup.Iterate())
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        if (pRenderState->allGpuState.pGraphicsPipeline != nullptr)
        {
            const uint64_t oldHash = pRenderState->allGpuState.pGraphicsPipeline->PalPipelineHash(deviceIdx);
            const uint64_t newHash = PalPipelineHash(deviceIdx);

            if ((oldHash != newHash)
                )
            {
                Pal::PipelineBindParams params = {};
                params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
                params.pPipeline         = m_pPalPipeline[deviceIdx];
                params.graphics          = graphicsShaderInfos;

                pPalCmdBuf->CmdBindPipeline(params);
            }
        }
        else
        {
            Pal::PipelineBindParams params = {};
            params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
            params.pPipeline         = m_pPalPipeline[deviceIdx];
            params.graphics          = graphicsShaderInfos;

            pPalCmdBuf->CmdBindPipeline(params);
        }

        // Bind state objects that are always static; these are redundancy checked by the pointer in the command buffer.
        pCmdBuffer->PalCmdBindDepthStencilState(pPalCmdBuf, deviceIdx, m_pPalDepthStencil[deviceIdx]);
        pCmdBuffer->PalCmdBindColorBlendState(pPalCmdBuf, deviceIdx, m_pPalColorBlend[deviceIdx]);
        pCmdBuffer->PalCmdBindMsaaState(pPalCmdBuf, deviceIdx, m_pPalMsaa[deviceIdx]);

        // Write parameters that are marked static pipeline state.  Redundancy check these based on static tokens:
        // skip the write if the previously written static token matches.
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.inputAssemblyState, newTokens.inputAssemblyState))
        {
            pPalCmdBuf->CmdSetInputAssemblyState(m_info.inputAssemblyState);
            pRenderState->allGpuState.staticTokens.inputAssemblyState = newTokens.inputAssemblyState;
        }

        if (CmdBuffer::IsStaticStateDifferent(oldTokens.triangleRasterState, newTokens.triangleRasterState))
        {
            pPalCmdBuf->CmdSetTriangleRasterState(m_info.triangleRasterState);
            pRenderState->allGpuState.staticTokens.triangleRasterState = newTokens.triangleRasterState;
        }

        if (PipelineSetsState(DynamicStatesInternal::LINE_WIDTH) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.pointLineRasterState, newTokens.pointLineRasterState))
        {
            pPalCmdBuf->CmdSetPointLineRasterState(m_info.pointLineRasterParams);
            pRenderState->allGpuState.staticTokens.pointLineRasterState = newTokens.pointLineRasterState;
        }

        if (PipelineSetsState(DynamicStatesInternal::DEPTH_BIAS) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBiasState, newTokens.depthBias))
        {
            pPalCmdBuf->CmdSetDepthBiasState(m_info.depthBiasParams);
            pRenderState->allGpuState.staticTokens.depthBiasState = newTokens.depthBias;
        }

        if (PipelineSetsState(DynamicStatesInternal::BLEND_CONSTANTS) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.blendConst, newTokens.blendConst))
        {
            pPalCmdBuf->CmdSetBlendConst(m_info.blendConstParams);
            pRenderState->allGpuState.staticTokens.blendConst = newTokens.blendConst;
        }

        if (PipelineSetsState(DynamicStatesInternal::DEPTH_BOUNDS) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBounds, newTokens.depthBounds))
        {
            pPalCmdBuf->CmdSetDepthBounds(m_info.depthBoundParams);
            pRenderState->allGpuState.staticTokens.depthBounds = newTokens.depthBounds;
        }

        if (PipelineSetsState(DynamicStatesInternal::SAMPLE_LOCATIONS_EXT) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.samplePattern, newTokens.samplePattern))
        {
            pCmdBuffer->PalCmdSetMsaaQuadSamplePattern(
                m_info.samplePattern.sampleCount, m_info.samplePattern.locations);
            pRenderState->allGpuState.staticTokens.samplePattern = newTokens.samplePattern;
        }
        // If we still need to rebind viewports but the pipeline state did not already do it, resend the state to PAL
        // (note that we are only reprogramming the previous state here, so no need to update tokens)
        if (viewportCountDirty)
        {
            pPalCmdBuf->CmdSetViewports(pRenderState->allGpuState.viewport);
        }

        if (scissorCountDirty)
        {
            pPalCmdBuf->CmdSetScissorRects(pRenderState->allGpuState.scissor);
        }
    }

    const bool stencilMasks = PipelineSetsState(DynamicStatesInternal::STENCIL_COMPARE_MASK) |
                              PipelineSetsState(DynamicStatesInternal::STENCIL_WRITE_MASK)   |
                              PipelineSetsState(DynamicStatesInternal::STENCIL_REFERENCE);

    // Until we expose Stencil Op Value, we always inherit the PSO value, which is currently Default == 1
    pStencilCombiner->Set(StencilRefMaskParams::FrontOpValue, m_info.stencilRefMasks.frontOpValue);
    pStencilCombiner->Set(StencilRefMaskParams::BackOpValue,  m_info.stencilRefMasks.backOpValue);

    if (stencilMasks)
    {
        // We don't have to use tokens for these since the combiner does a redundancy check on the full value
        if (PipelineSetsState(DynamicStatesInternal::STENCIL_COMPARE_MASK))
        {
            pStencilCombiner->Set(StencilRefMaskParams::FrontReadMask, m_info.stencilRefMasks.frontReadMask);
            pStencilCombiner->Set(StencilRefMaskParams::BackReadMask,  m_info.stencilRefMasks.backReadMask);
        }
        if (PipelineSetsState(DynamicStatesInternal::STENCIL_WRITE_MASK))
        {
            pStencilCombiner->Set(StencilRefMaskParams::FrontWriteMask, m_info.stencilRefMasks.frontWriteMask);
            pStencilCombiner->Set(StencilRefMaskParams::BackWriteMask,  m_info.stencilRefMasks.backWriteMask);
        }
        if (PipelineSetsState(DynamicStatesInternal::STENCIL_REFERENCE))
        {
            pStencilCombiner->Set(StencilRefMaskParams::FrontRef, m_info.stencilRefMasks.frontRef);
            pStencilCombiner->Set(StencilRefMaskParams::BackRef,  m_info.stencilRefMasks.backRef);
        }

        // Generate the PM4 if any of the Stencil state is to be statically bound
        // knowing we will likely overwrite it.
        pStencilCombiner->PalCmdSetStencilState(pCmdBuffer);
    }

    // Binding GraphicsPipeline affects ViewMask,
    // because when VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT is specified
    // ViewMask for each VkPhysicalDevice is defined by DeviceIndex
    // not by current subpass during a render pass instance.
    const bool oldViewIndexFromDeviceIndex = pRenderState->allGpuState.ViewIndexFromDeviceIndex;
    const bool newViewIndexFromDeviceIndex = ViewIndexFromDeviceIndex();
    const bool  isViewIndexFromDeviceIndexChanging = oldViewIndexFromDeviceIndex != newViewIndexFromDeviceIndex;
    if (isViewIndexFromDeviceIndexChanging)
    {
        // Update value of ViewIndexFromDeviceIndex for currently bound pipeline.
        pRenderState->allGpuState.ViewIndexFromDeviceIndex = newViewIndexFromDeviceIndex;

        // Sync ViewMask state in CommandBuffer.
        pCmdBuffer->SetViewInstanceMask();
    }
}

// =====================================================================================================================
// Binds a null pipeline to PAL
void GraphicsPipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numDevices = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;

    for (uint32_t deviceIdx = 0; deviceIdx < numDevices; deviceIdx++)
    {
        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        pPalCmdBuf->CmdBindPipeline(params);
        pPalCmdBuf->CmdBindMsaaState(nullptr);
        pPalCmdBuf->CmdBindColorBlendState(nullptr);
        pPalCmdBuf->CmdBindDepthStencilState(nullptr);
    }
}

} // namespace vk
