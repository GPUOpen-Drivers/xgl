/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "include/log.h"
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
#include "palInlineFuncs.h"
#include "palMetroHash.h"

#include <float.h>
#include <math.h>

using namespace Util;

namespace vk
{

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineVertexInputStateCreateInfo struct
// Pipeline compilation affected by:
//     - desc.pVertexBindingDescriptions
//     - desc.pVertexAttributeDescriptions
//     - pDivisorStateCreateInfo->pVertexBindingDivisors
void GraphicsPipeline::GenerateHashFromVertexInputStateCreateInfo(
    Util::MetroHash128*                         pHasher,
    const VkPipelineVertexInputStateCreateInfo& desc)
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
void GraphicsPipeline::GenerateHashFromInputAssemblyStateCreateInfo(
    Util::MetroHash128*                           pBaseHasher,
    Util::MetroHash128*                           pApiHasher,
    const VkPipelineInputAssemblyStateCreateInfo& desc)
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
void GraphicsPipeline::GenerateHashFromTessellationStateCreateInfo(
    Util::MetroHash128*                          pHasher,
    const VkPipelineTessellationStateCreateInfo& desc)
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
void GraphicsPipeline::GenerateHashFromViewportStateCreateInfo(
    Util::MetroHash128*                      pHasher,
    const VkPipelineViewportStateCreateInfo& desc,
    const uint32_t                           staticStateMask)
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
void GraphicsPipeline::GenerateHashFromRasterizationStateCreateInfo(
    Util::MetroHash128*                           pBaseHasher,
    Util::MetroHash128*                           pApiHasher,
    const VkPipelineRasterizationStateCreateInfo& desc)
{
    pBaseHasher->Update(desc.flags);
    pBaseHasher->Update(desc.depthClampEnable);
    pBaseHasher->Update(desc.rasterizerDiscardEnable);
    pBaseHasher->Update(desc.polygonMode);
    pBaseHasher->Update(desc.cullMode);
    pBaseHasher->Update(desc.frontFace);
    pBaseHasher->Update(desc.depthBiasEnable);
    pApiHasher->Update(desc.depthBiasConstantFactor);
    pApiHasher->Update(desc.depthBiasClamp);
    pApiHasher->Update(desc.depthBiasSlopeFactor);
    pApiHasher->Update(desc.lineWidth);

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
void GraphicsPipeline::GenerateHashFromMultisampleStateCreateInfo(
    Util::MetroHash128*                         pBaseHasher,
    Util::MetroHash128*                         pApiHasher,
    const VkPipelineMultisampleStateCreateInfo& desc)
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
void GraphicsPipeline::GenerateHashFromDepthStencilStateCreateInfo(
    Util::MetroHash128*                          pHasher,
    const VkPipelineDepthStencilStateCreateInfo& desc)
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
void GraphicsPipeline::GenerateHashFromColorBlendStateCreateInfo(
    Util::MetroHash128*                        pBaseHasher,
    Util::MetroHash128*                        pApiHasher,
    const VkPipelineColorBlendStateCreateInfo& desc)
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
            default:
                break;
            }

            pNext = pHeader->pNext;
        }
    }
}

// =====================================================================================================================
// Generates a hash using the contents of a VkPipelineDynamicStateCreateInfo struct
// Pipeline compilation affected by: none
void GraphicsPipeline::GenerateHashFromDynamicStateCreateInfo(
    Util::MetroHash128*                     pHasher,
    const VkPipelineDynamicStateCreateInfo& desc)
{
    pHasher->Update(desc.flags);
    pHasher->Update(desc.dynamicStateCount);

    for (uint32_t i = 0; i < desc.dynamicStateCount; i++)
    {
        pHasher->Update(desc.pDynamicStates[i]);
    }
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
uint64_t GraphicsPipeline::BuildApiHash(
    const VkGraphicsPipelineCreateInfo* pCreateInfo,
    const CreateInfo*                   pInfo,
    Util::MetroHash::Hash*              pBaseHash)
{
    Util::MetroHash128 baseHasher;
    Util::MetroHash128 apiHasher;

    baseHasher.Update(pCreateInfo->flags);
    baseHasher.Update(pCreateInfo->stageCount);

    for (uint32_t i = 0; i < pCreateInfo->stageCount; i++)
    {
        GenerateHashFromShaderStageCreateInfo(&baseHasher, pCreateInfo->pStages[i]);
    }

    if (pCreateInfo->pVertexInputState != nullptr)
    {
        GenerateHashFromVertexInputStateCreateInfo(&baseHasher, *pCreateInfo->pVertexInputState);
    }

    if (pCreateInfo->pInputAssemblyState != nullptr)
    {
        GenerateHashFromInputAssemblyStateCreateInfo(&baseHasher, &apiHasher, *pCreateInfo->pInputAssemblyState);
    }

    if ((pInfo->activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		&& (pCreateInfo->pTessellationState != nullptr))
    {
        GenerateHashFromTessellationStateCreateInfo(&baseHasher, *pCreateInfo->pTessellationState);
    }

    if ((pCreateInfo->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pCreateInfo->pViewportState != nullptr))
    {
        GenerateHashFromViewportStateCreateInfo(&apiHasher, *pCreateInfo->pViewportState, pInfo->staticStateMask);
    }

    if (pCreateInfo->pRasterizationState != nullptr)
    {
        GenerateHashFromRasterizationStateCreateInfo(&baseHasher, &apiHasher, *pCreateInfo->pRasterizationState);
    }

    if ((pCreateInfo->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pCreateInfo->pMultisampleState != nullptr))
    {
        GenerateHashFromMultisampleStateCreateInfo(&baseHasher, &apiHasher, *pCreateInfo->pMultisampleState);
    }

    if ((pCreateInfo->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pCreateInfo->pDepthStencilState != nullptr))
    {
        GenerateHashFromDepthStencilStateCreateInfo(&apiHasher, *pCreateInfo->pDepthStencilState);
    }

    if ((pCreateInfo->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pCreateInfo->pColorBlendState != nullptr))
    {
        GenerateHashFromColorBlendStateCreateInfo(&baseHasher, &apiHasher, *pCreateInfo->pColorBlendState);
    }

    if (pCreateInfo->pDynamicState != nullptr)
    {
        GenerateHashFromDynamicStateCreateInfo(&apiHasher, *pCreateInfo->pDynamicState);
    }

    baseHasher.Update(PipelineLayout::ObjectFromHandle(pCreateInfo->layout)->GetApiHash());
    baseHasher.Update(RenderPass::ObjectFromHandle(pCreateInfo->renderPass)->GetHash());
    baseHasher.Update(pCreateInfo->subpass);

    if ((pCreateInfo->flags & VK_PIPELINE_CREATE_DERIVATIVE_BIT) && (pCreateInfo->basePipelineHandle != VK_NULL_HANDLE))
    {
        apiHasher.Update(Pipeline::ObjectFromHandle(pCreateInfo->basePipelineHandle)->GetApiHash());
    }

    apiHasher.Update(pCreateInfo->basePipelineIndex);

    if (pCreateInfo->pNext != nullptr)
    {
        const void* pNext = pCreateInfo->pNext;

        while (pNext != nullptr)
        {
            const auto* pHeader = static_cast<const VkStructHeader*>(pNext);

            switch (static_cast<uint32>(pHeader->sType))
            {
            case VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_ADVANCED_STATE_CREATE_INFO_EXT:
            {
                const auto* pExtInfo = static_cast<const VkPipelineDiscardRectangleStateCreateInfoEXT*>(pNext);
                apiHasher.Update(pExtInfo->sType);
                apiHasher.Update(pExtInfo->flags);
                apiHasher.Update(pExtInfo->discardRectangleMode);
                apiHasher.Update(pExtInfo->discardRectangleCount);

                if (pExtInfo->pDiscardRectangles != nullptr)
                {
                    for (uint32 i = 0; i < pExtInfo->discardRectangleCount; i++)
                    {
                        apiHasher.Update(pExtInfo->pDiscardRectangles[i]);
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

    baseHasher.Finalize(reinterpret_cast<uint8_t* const>(pBaseHash));

    uint64_t              apiHash;
    Util::MetroHash::Hash apiHashFull;
    apiHasher.Update(*pBaseHash);
    apiHasher.Finalize(reinterpret_cast<uint8_t* const>(&apiHashFull));
    apiHash = Util::MetroHash::Compact64(&apiHashFull);

    return apiHash;
}

// =====================================================================================================================
// Returns true if the given VkBlendFactor factor is a dual source blend factor
bool IsDualSourceBlend(VkBlendFactor blend)
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
// Returns true if Dual Source Blending is to be enabled based on the given ColorBlendAttachmentState
bool GetDualSourceBlendEnableState(const VkPipelineColorBlendAttachmentState& pColorBlendAttachmentState)
{
    bool dualSourceBlend = false;

    dualSourceBlend |= IsDualSourceBlend(pColorBlendAttachmentState.srcAlphaBlendFactor);
    dualSourceBlend |= IsDualSourceBlend(pColorBlendAttachmentState.dstAlphaBlendFactor);
    dualSourceBlend |= IsDualSourceBlend(pColorBlendAttachmentState.srcColorBlendFactor);
    dualSourceBlend |= IsDualSourceBlend(pColorBlendAttachmentState.dstColorBlendFactor);
    dualSourceBlend &= (pColorBlendAttachmentState.blendEnable == VK_TRUE);

    return dualSourceBlend;
}

// =====================================================================================================================
// Parses input pipeline rasterization create info state.
void GraphicsPipeline::BuildRasterizationState(
    Device*                                       pDevice,
    const VkPipelineRasterizationStateCreateInfo* pIn,
    CreateInfo*                                   pInfo,
    const bool                                    dynamicStateFlags[])
{
    VK_ASSERT(pIn->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);

    // By default rasterization is disabled, unless rasterization creation info is present

    const PhysicalDevice*         pPhysicalDevice = pDevice->VkPhysicalDevice(DefaultDeviceIndex);
    const VkPhysicalDeviceLimits& limits          = pPhysicalDevice->GetLimits();

    // Enable perpendicular end caps if we report strictLines semantics
    pInfo->pipeline.rsState.perpLineEndCapsEnable = (limits.strictLines == VK_TRUE);

    // For optimal performance, depth clamping should be enabled by default. Only disable it if dealing
    // with depth values outside of [0.0, 1.0] range.
    // Note that this is the opposite of the default Vulkan setting which is depthClampEnable = false.
    if ((pIn->depthClampEnable == VK_FALSE) &&
        (pDevice->IsExtensionEnabled(DeviceExtensions::EXT_DEPTH_RANGE_UNRESTRICTED)))
    {
        pInfo->pipeline.rsState.depthClampDisable = true;
    }
    else
    {
        // When depth clamping is enabled, depth clipping should be disabled, and vice versa.
        // Clipping is updated in pipeline compiler.
        pInfo->pipeline.rsState.depthClampDisable = false;
    }

    pInfo->pipeline.viewportInfo.depthClipEnable                = (pIn->depthClampEnable == VK_FALSE);
    pInfo->pipeline.viewportInfo.depthRange                     = Pal::DepthRange::ZeroToOne;

    pInfo->immedInfo.triangleRasterState.frontFillMode          = VkToPalFillMode(pIn->polygonMode);
    pInfo->immedInfo.triangleRasterState.backFillMode           = VkToPalFillMode(pIn->polygonMode);
    pInfo->immedInfo.triangleRasterState.cullMode               = VkToPalCullMode(pIn->cullMode);
    pInfo->immedInfo.triangleRasterState.frontFace              = VkToPalFaceOrientation(pIn->frontFace);

    pInfo->immedInfo.triangleRasterState.flags.depthBiasEnable  = pIn->depthBiasEnable;
    pInfo->immedInfo.depthBiasParams.depthBias                  = pIn->depthBiasConstantFactor;
    pInfo->immedInfo.depthBiasParams.depthBiasClamp             = pIn->depthBiasClamp;
    pInfo->immedInfo.depthBiasParams.slopeScaledDepthBias       = pIn->depthBiasSlopeFactor;

    if (pIn->depthBiasEnable && (dynamicStateFlags[VK_DYNAMIC_STATE_DEPTH_BIAS] == false))
    {
        pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_DEPTH_BIAS;
    }

    // point size must be set via gl_PointSize, otherwise it must be 1.0f.
    constexpr float DefaultPointSize = 1.0f;

    pInfo->immedInfo.pointLineRasterParams.lineWidth    = pIn->lineWidth;
    pInfo->immedInfo.pointLineRasterParams.pointSize    = DefaultPointSize;
    pInfo->immedInfo.pointLineRasterParams.pointSizeMin = limits.pointSizeRange[0];
    pInfo->immedInfo.pointLineRasterParams.pointSizeMax = limits.pointSizeRange[1];

    if (dynamicStateFlags[VK_DYNAMIC_STATE_LINE_WIDTH] == false)
    {
        pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_LINE_WIDTH;
    }

    const void* pNext = pIn->pNext;

    while (pNext != nullptr)
    {
        const VkStructHeader* pHeader = static_cast<const VkStructHeader*>(pNext);

        switch (static_cast<int32>(pHeader->sType))
        {
        // Handle  extension specific structures
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD:
            {
                const auto* pRsOrder = static_cast<const VkPipelineRasterizationStateRasterizationOrderAMD*>(pNext);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 493
                if (pPhysicalDevice->PalProperties().gfxipProperties.flags.supportOutOfOrderPrimitives)
#endif
                {
                    pInfo->pipeline.rsState.outOfOrderPrimsEnable = VkToPalRasterizationOrder(pRsOrder->rasterizationOrder);
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
                VK_ASSERT(pRsConservative->conservativeRasterizationMode >= VK_CONSERVATIVE_RASTERIZATION_MODE_BEGIN_RANGE_EXT);
                VK_ASSERT(pRsConservative->conservativeRasterizationMode <= VK_CONSERVATIVE_RASTERIZATION_MODE_END_RANGE_EXT);
                VK_IGNORE(pRsConservative->extraPrimitiveOverestimationSize);

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
                        pInfo->msaa.conservativeRasterizationMode = Pal::ConservativeRasterizationMode::Overestimate;
                    }
                    break;
                case VK_CONSERVATIVE_RASTERIZATION_MODE_UNDERESTIMATE_EXT:
                    {
                        pInfo->msaa.flags.enableConservativeRasterization = true;
                        pInfo->msaa.conservativeRasterizationMode = Pal::ConservativeRasterizationMode::Underestimate;
                    }
                    break;

                default:
                    break;
                }

            }
            break;
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_STREAM_CREATE_INFO_EXT:
            {
                const auto* pRsStream = static_cast<const VkPipelineRasterizationStateStreamCreateInfoEXT*>(pNext);

                pInfo->rasterizationStream = pRsStream->rasterizationStream;
            }
            break;
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT:
            {
                const auto* pRsRasterizationLine =
                    static_cast<const VkPipelineRasterizationLineStateCreateInfoEXT*>(pNext);

                pInfo->bresenhamEnable =
                    (pRsRasterizationLine->lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT);

                // Bresenham Lines need axis aligned end caps
                if (pInfo->bresenhamEnable)
                {
                    pInfo->pipeline.rsState.perpLineEndCapsEnable = false;
                }
                else if (pRsRasterizationLine->lineRasterizationMode == VK_LINE_RASTERIZATION_MODE_RECTANGULAR_EXT)
                {
                    pInfo->pipeline.rsState.perpLineEndCapsEnable = true;
                }

                pInfo->msaa.flags.enableLineStipple                   = pRsRasterizationLine->stippledLineEnable;

                pInfo->immedInfo.lineStippleParams.lineStippleScale   = (pRsRasterizationLine->lineStippleFactor - 1);
                pInfo->immedInfo.lineStippleParams.lineStippleValue   = pRsRasterizationLine->lineStipplePattern;

                if (pRsRasterizationLine->stippledLineEnable &&
                    (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt)] == false))
                {
                    pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt);
                }
            }
            break;
        case VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_DEPTH_CLIP_STATE_CREATE_INFO_EXT:
            {
                const auto* pRsRasterizationDepthClipState =
                    static_cast<const VkPipelineRasterizationDepthClipStateCreateInfoEXT*>(pNext);

                pInfo->pipeline.viewportInfo.depthClipEnable = (pRsRasterizationDepthClipState->depthClipEnable == VK_TRUE);
            }
            break;

        default:
            // Skip any unknown extension structures
            break;
        }

        pNext = pHeader->pNext;
    }
}

// =====================================================================================================================
// Converts Vulkan graphics pipeline parameters to an internal structure
void GraphicsPipeline::ConvertGraphicsPipelineInfo(
    Device*                             pDevice,
    const VkGraphicsPipelineCreateInfo* pIn,
    const VbBindingInfo*                pVbInfo,
    CreateInfo*                         pInfo)
{
    const RuntimeSettings& settings = pDevice->GetRuntimeSettings();
    VkFormat cbFormat[Pal::MaxColorTargets] = {};

    // Fill in necessary non-zero defaults in case some information is missing
    pInfo->msaa.coverageSamples                 = 1;
    pInfo->msaa.pixelShaderSamples              = 1;
    pInfo->msaa.depthStencilSamples             = 1;
    pInfo->msaa.shaderExportMaskSamples         = 1;
    pInfo->msaa.sampleClusters                  = 1;
    pInfo->msaa.alphaToCoverageSamples          = 1;
    pInfo->msaa.occlusionQuerySamples           = 1;
    pInfo->msaa.sampleMask                      = 1;
    pInfo->sampleCoverage                       = 1;
    pInfo->rasterizationStream                  = 0;

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
        pInfo->immedInfo.inputAssemblyState.topology               = VkToPalPrimitiveTopology(pIa->topology);

        pInfo->pipeline.iaState.vertexBufferCount                  = pVbInfo->bindingTableSize;

        pInfo->pipeline.iaState.topologyInfo.primitiveType         = VkToPalPrimitiveType(pIa->topology);

        if (pInfo->activeStages & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
        {
            EXTRACT_VK_STRUCTURES_0(
                Tess,
                PipelineTessellationStateCreateInfo,
                pGraphicsPipelineCreateInfo->pTessellationState,
                PIPELINE_TESSELLATION_STATE_CREATE_INFO)

                if (pPipelineTessellationStateCreateInfo != nullptr)
                {
                    pInfo->pipeline.iaState.topologyInfo.patchControlPoints = pPipelineTessellationStateCreateInfo->patchControlPoints;
                }
        }
        pInfo->staticStateMask = 0;

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
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt)] = true;
                        break;

                    case VK_DYNAMIC_STATE_LINE_STIPPLE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::LineStippleExt)] = true;
                        break;

                    case  VK_DYNAMIC_STATE_CULL_MODE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::CullModeExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_FRONT_FACE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt)] = true;
                        break;
                    case VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::ViewportCount)] = true;
                        dynamicStateFlags[VK_DYNAMIC_STATE_VIEWPORT]                                    = true;
                        break;
                    case VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::ScissorCount)] = true;
                        dynamicStateFlags[VK_DYNAMIC_STATE_SCISSOR]                                    = true;
                        break;
                    case  VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt)]
                            = true;
                        break;
                    case  VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt)]
                            = true;
                        break;
                    case  VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt)] = true;
                        break;
                    case  VK_DYNAMIC_STATE_STENCIL_OP_EXT:
                        dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt)] = true;
                        break;

                    default:
                        // skip unknown dynamic state
                        break;
                    }
                }
            }
        }

        pInfo->bindDepthStencilObject  = true;
        pInfo->bindTriangleRasterState = true;
        pInfo->bindStencilRefMasks     = true;
        pInfo->bindInputAssemblyState  = true;

        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::CullModeExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::CullModeExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::ViewportCount)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::ViewportCount);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::ScissorCount)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::ScissorCount);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::VertexInputBindingStrideExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt);
        }
        if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt)] == false)
        {
            pInfo->staticStateMask |= 1 << static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt);
        }

        pInfo->bindDepthStencilObject =
            !(dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilOpExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilTestEnableExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthCompareOpExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthWriteEnableExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthTestEnableExt)]);

        pInfo->bindTriangleRasterState =
            !(dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::CullModeExt)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::FrontFaceExt)]);

        pInfo->bindStencilRefMasks =
            !(dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilCompareMask)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilWriteMask)] ||
              dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::StencilReference)]);

        pInfo->bindInputAssemblyState =
            !dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::PrimitiveTopologyExt)];

        const VkPipelineViewportStateCreateInfo* pVp = pGraphicsPipelineCreateInfo->pViewportState;

        if ((pIn->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pVp != nullptr))
        {
            // From the spec, "scissorCount is the number of scissors and must match the number of viewports."
            VK_ASSERT(pVp->viewportCount <= Pal::MaxViewports);
            VK_ASSERT(pVp->scissorCount <= Pal::MaxViewports);
            VK_ASSERT(pVp->scissorCount == pVp->viewportCount);

            pInfo->immedInfo.viewportParams.count    = pVp->viewportCount;
            pInfo->immedInfo.scissorRectParams.count = pVp->scissorCount;

            if (dynamicStateFlags[VK_DYNAMIC_STATE_VIEWPORT] == false)
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

                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_VIEWPORT;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_SCISSOR] == false)
            {
                VK_ASSERT(pVp->pScissors != nullptr);

                for (uint32_t i = 0; i < pVp->scissorCount; ++i)
                {
                    VkToPalScissorRect(pVp->pScissors[i], i, &pInfo->immedInfo.scissorRectParams);
                }
                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_SCISSOR;
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

        const VkPipelineMultisampleStateCreateInfo* pMs = pGraphicsPipelineCreateInfo->pMultisampleState;

        if ((pIn->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pMs != nullptr))
        {
            // Sample Locations
            EXTRACT_VK_STRUCTURES_1(
                SampleLocations,
                PipelineMultisampleStateCreateInfo,
                PipelineSampleLocationsStateCreateInfoEXT,
                pMs,
                PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                PIPELINE_SAMPLE_LOCATIONS_STATE_CREATE_INFO_EXT)

            pInfo->customSampleLocations = ((pPipelineSampleLocationsStateCreateInfoEXT != nullptr) &&
                                            (pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsEnable));

            if ((pInfo->bresenhamEnable == false) || pInfo->customSampleLocations)
            {
                VK_ASSERT(pRenderPass != nullptr);

                uint32_t rasterizationSampleCount   = pMs->rasterizationSamples;
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

                if (pInfo->customSampleLocations)
                {
                    // Enable single-sampled custom sample locations if necessary
                    pInfo->msaa.flags.enable1xMsaaSampleLocations = (pInfo->msaa.coverageSamples == 1);

                    if (dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt)] == false)
                    {
                        // We store the custom sample locations if custom sample locations are enabled and the
                        // sample locations state is static.
                        pInfo->immedInfo.samplePattern.sampleCount =
                            (uint32_t)pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo.sampleLocationsPerPixel;

                        ConvertToPalMsaaQuadSamplePattern(
                            &pPipelineSampleLocationsStateCreateInfoEXT->sampleLocationsInfo,
                            &pInfo->immedInfo.samplePattern.locations);

                        VK_ASSERT(pInfo->immedInfo.samplePattern.sampleCount == rasterizationSampleCount);

                        pInfo->staticStateMask |=
                            (1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt));
                    }
                }
                else
                {
                    // We store the standard sample locations if custom sample locations are not enabled.
                    pInfo->immedInfo.samplePattern.sampleCount = rasterizationSampleCount;
                    pInfo->immedInfo.samplePattern.locations =
                        *Device::GetDefaultQuadSamplePattern(rasterizationSampleCount);

                    pInfo->staticStateMask |=
                        1 << static_cast<uint32_t>(DynamicStatesInternal::SampleLocationsExt);
                }
            }

            // The alpha component of the fragment's first color output is replaced with one if alphaToOneEnable is set.
            pInfo->pipeline.cbState.target[0].forceAlphaToOne = (pMs->alphaToOneEnable == VK_TRUE);
            pInfo->pipeline.cbState.alphaToCoverageEnable     = (pMs->alphaToCoverageEnable == VK_TRUE);
        }

        const VkPipelineColorBlendStateCreateInfo* pCb = pGraphicsPipelineCreateInfo->pColorBlendState;

        bool blendingEnabled = false;
        bool dualSourceBlend = false;

        if ((pIn->pRasterizationState->rasterizerDiscardEnable == VK_TRUE) || (pCb == nullptr))
        {
            pInfo->pipeline.cbState.logicOp = Pal::LogicOp::Copy;
        }
        else
        {
            pInfo->pipeline.cbState.logicOp = (pCb->logicOpEnable) ?
                                              VkToPalLogicOp(pCb->logicOp) :
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
                    pCbDst->swizzledFormat = VkToPalFormat(cbFormat[i], pDevice->GetRuntimeSettings());
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

                dualSourceBlend |= GetDualSourceBlendEnableState(src);
            }
        }

        pInfo->pipeline.cbState.dualSourceBlendEnable     = dualSourceBlend;

        if (blendingEnabled == true && dynamicStateFlags[VK_DYNAMIC_STATE_BLEND_CONSTANTS] == false)
        {
            static_assert(sizeof(pInfo->immedInfo.blendConstParams) == sizeof(pCb->blendConstants),
                "Blend constant structure size mismatch!");

            memcpy(&pInfo->immedInfo.blendConstParams, pCb->blendConstants, sizeof(pCb->blendConstants));

            pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_BLEND_CONSTANTS;
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
            pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = (pDs->stencilTestEnable == VK_TRUE);
            pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = (pDs->depthTestEnable == VK_TRUE);
            pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = (pDs->depthWriteEnable == VK_TRUE);
            pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = VkToPalCompareFunc(pDs->depthCompareOp);
            pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = (pDs->depthBoundsTestEnable == VK_TRUE);

            if ((pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable ||
                 dynamicStateFlags[static_cast<uint32_t>(DynamicStatesInternal::DepthBoundsTestEnableExt)]) &&
                (dynamicStateFlags[VK_DYNAMIC_STATE_DEPTH_BOUNDS] == false))
            {
                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_DEPTH_BOUNDS;
            }

            // According to Graham, we should program the stencil state at PSO bind time,
            // regardless of whether this PSO enables\disables Stencil. This allows a second PSO
            // to inherit the first PSO's settings
            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK] == false)
            {
                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_WRITE_MASK] == false)
            {
                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_WRITE_MASK;
            }

            if (dynamicStateFlags[VK_DYNAMIC_STATE_STENCIL_REFERENCE] == false)
            {
                pInfo->staticStateMask |= 1 << VK_DYNAMIC_STATE_STENCIL_REFERENCE;
            }
        }
        else
        {
            pInfo->immedInfo.depthStencilCreateInfo.depthEnable       = false;
            pInfo->immedInfo.depthStencilCreateInfo.depthWriteEnable  = false;
            pInfo->immedInfo.depthStencilCreateInfo.depthFunc         = Pal::CompareFunc::Always;
            pInfo->immedInfo.depthStencilCreateInfo.depthBoundsEnable = false;
            pInfo->immedInfo.depthStencilCreateInfo.stencilEnable     = false;
        }

        if ((pIn->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pDs != nullptr))
        {
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
    int64_t                     startTime                           = Util::GetPerfCpuTime();
    // Parse the create info and build patched AMDIL shaders
    CreateInfo                  localPipelineInfo                  = {};
    VbBindingInfo               vbInfo                             = {};
    GraphicsPipelineCreateInfo  binaryCreateInfo                   = {};
    size_t                      pipelineBinarySizes[MaxPalDevices] = {};
    const void*                 pPipelineBinaries[MaxPalDevices]   = {};
    Util::MetroHash::Hash       cacheId[MaxPalDevices]             = {};
    Pal::Result                 palResult                          = Pal::Result::Success;
    PipelineCompiler*           pDefaultCompiler                   = pDevice->GetCompiler(DefaultDeviceIndex);
    const RuntimeSettings&      settings                           = pDevice->GetRuntimeSettings();
    Util::MetroHash64           palPipelineHasher;

    const VkPipelineCreationFeedbackCreateInfoEXT* pPipelineCreationFeadbackCreateInfo = nullptr;

    VkResult result = pDefaultCompiler->ConvertGraphicsPipelineInfo(
        pDevice, pCreateInfo, &binaryCreateInfo, &vbInfo, &pPipelineCreationFeadbackCreateInfo);
    ConvertGraphicsPipelineInfo(pDevice, pCreateInfo, &vbInfo, &localPipelineInfo);
    uint64_t apiPsoHash = BuildApiHash(pCreateInfo, &localPipelineInfo, &binaryCreateInfo.basePipelineHash);

    const uint32_t numPalDevices = pDevice->NumPalDevices();
    uint64_t pipelineHash = Vkgc::IPipelineDumper::GetPipelineHash(&binaryCreateInfo.pipelineInfo);
    for (uint32_t i = 0; (result == VK_SUCCESS) && (i < numPalDevices); ++i)
    {
        if (i == DefaultDeviceIndex)
        {
            result = pDevice->GetCompiler(i)->CreateGraphicsPipelineBinary(
                pDevice,
                i,
                pPipelineCache,
                &binaryCreateInfo,
                &pipelineBinarySizes[i],
                &pPipelineBinaries[i],
                localPipelineInfo.rasterizationStream,
                &cacheId[i]);
        }
        else
        {
            GraphicsPipelineCreateInfo binaryCreateInfoMGPU = {};
            VbBindingInfo vbInfoMGPU = {};
            pDefaultCompiler->ConvertGraphicsPipelineInfo(
                    pDevice, pCreateInfo, &binaryCreateInfoMGPU, &vbInfoMGPU, nullptr);

            result = pDevice->GetCompiler(i)->CreateGraphicsPipelineBinary(
                pDevice,
                i,
                pPipelineCache,
                &binaryCreateInfoMGPU,
                &pipelineBinarySizes[i],
                &pPipelineBinaries[i],
                localPipelineInfo.rasterizationStream,
                &cacheId[i]);

            if (result == VK_SUCCESS)
            {
                pDefaultCompiler->SetPipelineCreationFeedbackInfo(
                    pPipelineCreationFeadbackCreateInfo,
                    &binaryCreateInfoMGPU.pipelineFeedback);
            }

            pDefaultCompiler->FreeGraphicsPipelineCreateInfo(&binaryCreateInfoMGPU);
        }
    }

    if (result == VK_SUCCESS)
    {
        pDevice->GetShaderOptimizer()->OverrideGraphicsPipelineCreateInfo(
            binaryCreateInfo.pipelineProfileKey,
            localPipelineInfo.activeStages,
            &localPipelineInfo.pipeline,
            &localPipelineInfo.immedInfo.graphicsShaderInfos);

        palPipelineHasher.Update(localPipelineInfo.pipeline);
    }

    RenderStateCache* pRSCache = pDevice->GetRenderStateCache();

    // Get the pipeline size from PAL and allocate memory.
    void*  pSystemMem = nullptr;
    size_t palSize    = 0;

    if (result == VK_SUCCESS)
    {
        localPipelineInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[DefaultDeviceIndex];
        localPipelineInfo.pipeline.pPipelineBinary    = pPipelineBinaries[DefaultDeviceIndex];

        palSize =
            pDevice->PalDevice(DefaultDeviceIndex)->GetGraphicsPipelineSize(localPipelineInfo.pipeline, &palResult);
        VK_ASSERT(palResult == Pal::Result::Success);

        pSystemMem = pDevice->AllocApiObject(
            pAllocator,
            sizeof(GraphicsPipeline) + (palSize * numPalDevices));

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
                    localPipelineInfo.pipeline.pipelineBinarySize = pipelineBinarySizes[deviceIdx];
                    localPipelineInfo.pipeline.pPipelineBinary    = pPipelineBinaries[deviceIdx];
                }

                palResult = pPalDevice->CreateGraphicsPipeline(
                    localPipelineInfo.pipeline,
                    Util::VoidPtrInc(pSystemMem, palOffset),
                    &pPalPipeline[deviceIdx]);

#if ICD_GPUOPEN_DEVMODE_BUILD
                // Temporarily reinject post Pal pipeline creation (when the internal pipeline hash is available).
                // The reinjection cache layer can be linked back into the pipeline cache chain once the
                // Vulkan pipeline cache key can be stored (and read back) inside the ELF as metadata.
                if ((pDevice->VkInstance()->GetDevModeMgr() != nullptr) &&
                    (palResult == Util::Result::Success))
                {
                    const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                    palResult = pDevice->GetCompiler(deviceIdx)->RegisterAndLoadReinjectionBinary(
                        &info.internalPipelineHash,
                        &cacheId[deviceIdx],
                        &localPipelineInfo.pipeline.pipelineBinarySize,
                        &localPipelineInfo.pipeline.pPipelineBinary,
                        pPipelineCache);

                    if (palResult == Util::Result::Success)
                    {
                        pPalPipeline[deviceIdx]->Destroy();

                        palResult = pPalDevice->CreateGraphicsPipeline(
                            localPipelineInfo.pipeline,
                            Util::VoidPtrInc(pSystemMem, palOffset),
                            &pPalPipeline[deviceIdx]);
                    }
                    else if (palResult == Util::Result::NotFound)
                    {
                        // If a replacement was not found, proceed with the original
                        palResult = Util::Result::Success;
                    }
                }
#endif

                VK_ASSERT(palSize == pPalDevice->GetGraphicsPipelineSize(localPipelineInfo.pipeline, nullptr));
                palOffset += palSize;
            }

            // Create the PAL MSAA state object
            if (palResult == Pal::Result::Success)
            {
                const auto& pMs = pCreateInfo->pMultisampleState;

                // Force full sample shading if the app didn't enable it, but the shader wants
                // per-sample shading by the use of SampleId or similar features.
                if ((pCreateInfo->pRasterizationState->rasterizerDiscardEnable != VK_TRUE) && (pMs != nullptr) &&
                    (pMs->sampleShadingEnable == false))
                {
                    const auto& info = pPalPipeline[deviceIdx]->GetInfo();

                    if (info.ps.flags.perSampleShading == 1)
                    {
                        localPipelineInfo.msaa.pixelShaderSamples = localPipelineInfo.msaa.coverageSamples;
                    }
                }

                palResult = pRSCache->CreateMsaaState(
                    localPipelineInfo.msaa,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalMsaa);
            }

            // Create the PAL color blend state object
            if (palResult == Pal::Result::Success)
            {
                palResult = pRSCache->CreateColorBlendState(
                    localPipelineInfo.blend,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalColorBlend);
            }

            // Create the PAL depth stencil state object
            if ((palResult == Pal::Result::Success) && localPipelineInfo.bindDepthStencilObject)
            {
                palResult = pRSCache->CreateDepthStencilState(
                    localPipelineInfo.immedInfo.depthStencilCreateInfo,
                    pAllocator,
                    VK_SYSTEM_ALLOCATION_SCOPE_OBJECT,
                    pPalDepthStencil);
            }

            // Reset the PAL stencil maskupdate flags
            localPipelineInfo.immedInfo.stencilRefMasks.flags.u8All = 0xff;
        }

        result = PalToVkResult(palResult);
    }

    PipelineBinaryInfo* pBinaryInfo = nullptr;

    if ((pDevice->IsExtensionEnabled(DeviceExtensions::AMD_SHADER_INFO) ||
        (pDevice->IsExtensionEnabled(DeviceExtensions::KHR_PIPELINE_EXECUTABLE_PROPERTIES) &&
        ((pCreateInfo->flags & VK_PIPELINE_CREATE_CAPTURE_INTERNAL_REPRESENTATIONS_BIT_KHR) != 0))) &&
        (result == VK_SUCCESS))
    {
        pBinaryInfo = PipelineBinaryInfo::Create(
            pipelineBinarySizes[DefaultDeviceIndex],
            pPipelineBinaries[DefaultDeviceIndex],
            pAllocator);
    }

    const bool viewIndexFromDeviceIndex = Util::TestAnyFlagSet(
        pCreateInfo->flags,
        VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT);

    // On success, wrap it up in a Vulkan object.
    if (result == VK_SUCCESS)
    {
        VK_PLACEMENT_NEW(pSystemMem) GraphicsPipeline(
            pDevice,
            pPalPipeline,
            localPipelineInfo.pLayout,
            localPipelineInfo.immedInfo,
            localPipelineInfo.staticStateMask,
            localPipelineInfo.bindDepthStencilObject,
            localPipelineInfo.bindTriangleRasterState,
            localPipelineInfo.bindStencilRefMasks,
            localPipelineInfo.bindInputAssemblyState,
            localPipelineInfo.customSampleLocations,
            vbInfo,
            pPalMsaa,
            pPalColorBlend,
            pPalDepthStencil,
            localPipelineInfo.sampleCoverage,
            viewIndexFromDeviceIndex,
            pBinaryInfo,
            apiPsoHash,
            &palPipelineHasher);

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

        if (pPalDepthStencil[0] != nullptr)
        {
            pRSCache->DestroyDepthStencilState(pPalDepthStencil, pAllocator);
        }

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

        pDevice->FreeApiObject(pAllocator, pSystemMem);
    }
    if (result == VK_SUCCESS)
    {
        uint64_t duration = Util::GetPerfCpuTime() - startTime;
        binaryCreateInfo.pipelineFeedback.feedbackValid = true;
        binaryCreateInfo.pipelineFeedback.duration = duration;
        pDefaultCompiler->SetPipelineCreationFeedbackInfo(
            pPipelineCreationFeadbackCreateInfo,
            &binaryCreateInfo.pipelineFeedback);

        // The hash is same as pipline dump file name, we can easily analyze further.
        AmdvlkLog(settings.logTagIdMask, PipelineCompileTime, "0x%016llX-%llu", pipelineHash, duration);
    }

    return result;
}

// =====================================================================================================================
GraphicsPipeline::GraphicsPipeline(
    Device* const                          pDevice,
    Pal::IPipeline**                       pPalPipeline,
    const PipelineLayout*                  pLayout,
    const ImmedInfo&                       immedInfo,
    uint32_t                               staticStateMask,
    bool                                   bindDepthStencilObject,
    bool                                   bindTriangleRasterState,
    bool                                   bindStencilRefMasks,
    bool                                   bindInputAssemblyState,
    bool                                   customSampleLocations,
    const VbBindingInfo&                   vbInfo,
    Pal::IMsaaState**                      pPalMsaa,
    Pal::IColorBlendState**                pPalColorBlend,
    Pal::IDepthStencilState**              pPalDepthStencil,
    uint32_t                               coverageSamples,
    bool                                   viewIndexFromDeviceIndex,
    PipelineBinaryInfo*                    pBinary,
    uint64_t                               apiHash,
    Util::MetroHash64*                     pPalPipelineHasher)
    :
    Pipeline(pDevice),
    m_info(immedInfo),
    m_vbInfo(vbInfo),
    m_flags()
{
    Pipeline::Init(pPalPipeline, pLayout, pBinary, staticStateMask, apiHash);

    memcpy(m_pPalMsaa,         pPalMsaa,         sizeof(pPalMsaa[0])         * pDevice->NumPalDevices());
    memcpy(m_pPalColorBlend,   pPalColorBlend,   sizeof(pPalColorBlend[0])   * pDevice->NumPalDevices());
    memcpy(m_pPalDepthStencil, pPalDepthStencil, sizeof(pPalDepthStencil[0]) * pDevice->NumPalDevices());

    m_flags.viewIndexFromDeviceIndex = viewIndexFromDeviceIndex;
    m_flags.bindDepthStencilObject   = bindDepthStencilObject;
    m_flags.bindTriangleRasterState  = bindTriangleRasterState;
    m_flags.bindStencilRefMasks      = bindStencilRefMasks;
    m_flags.bindInputAssemblyState   = bindInputAssemblyState;
	m_flags.customSampleLocations    = customSampleLocations;
    CreateStaticState();

    pPalPipelineHasher->Update(m_palPipelineHash);
    pPalPipelineHasher->Finalize(reinterpret_cast<uint8* const>(&m_palPipelineHash));
}

// =====================================================================================================================
// Creates instances of static pipeline state.  Much of this information can be cached at the device-level to help speed
// up pipeline-bind operations.
void GraphicsPipeline::CreateStaticState()
{
    RenderStateCache* pCache = m_pDevice->GetRenderStateCache();
    auto* pStaticTokens      = &m_info.staticTokens;

    pStaticTokens->inputAssemblyState         = DynamicRenderStateToken;
    pStaticTokens->triangleRasterState        = DynamicRenderStateToken;
    pStaticTokens->pointLineRasterState       = DynamicRenderStateToken;
    pStaticTokens->depthBias                  = DynamicRenderStateToken;
    pStaticTokens->blendConst                 = DynamicRenderStateToken;
    pStaticTokens->depthBounds                = DynamicRenderStateToken;
    pStaticTokens->viewport                   = DynamicRenderStateToken;
    pStaticTokens->scissorRect                = DynamicRenderStateToken;
    pStaticTokens->samplePattern              = DynamicRenderStateToken;
    pStaticTokens->lineStippleState           = DynamicRenderStateToken;

    if (m_flags.bindInputAssemblyState)
    {
        pStaticTokens->inputAssemblyState = pCache->CreateInputAssemblyState(m_info.inputAssemblyState);
    }

    if (m_flags.bindTriangleRasterState)
    {
        pStaticTokens->triangleRasterState = pCache->CreateTriangleRasterState(m_info.triangleRasterState);
    }

    if (ContainsStaticState(DynamicStatesInternal::LineWidth))
    {
        pStaticTokens->pointLineRasterState = pCache->CreatePointLineRasterState(
            m_info.pointLineRasterParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::DepthBias))
    {
        pStaticTokens->depthBias = pCache->CreateDepthBias(m_info.depthBiasParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::BlendConstants))
    {
        pStaticTokens->blendConst = pCache->CreateBlendConst(m_info.blendConstParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::DepthBounds))
    {
        pStaticTokens->depthBounds = pCache->CreateDepthBounds(m_info.depthBoundParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::Viewport))
    {
        pStaticTokens->viewport = pCache->CreateViewport(m_info.viewportParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::Scissor))
    {
        pStaticTokens->scissorRect = pCache->CreateScissorRect(m_info.scissorRectParams);
    }

    if (ContainsStaticState(DynamicStatesInternal::SampleLocationsExt))
    {
        pStaticTokens->samplePattern = pCache->CreateSamplePattern(m_info.samplePattern);
    }

    if (ContainsStaticState(DynamicStatesInternal::LineStippleExt))
    {
        pStaticTokens->lineStippleState = pCache->CreateLineStipple(m_info.lineStippleParams);
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

    if (m_pPalDepthStencil[0] != nullptr)
    {
        pCache->DestroyDepthStencilState(m_pPalDepthStencil, pAllocator);
    }

    if (m_flags.bindInputAssemblyState)
    {
        pCache->DestroyInputAssemblyState(m_info.inputAssemblyState,
                                          m_info.staticTokens.inputAssemblyState);
    }

    if (m_flags.bindTriangleRasterState)
    {
        pCache->DestroyTriangleRasterState(m_info.triangleRasterState,
                                           m_info.staticTokens.triangleRasterState);
    }

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
    CmdBufferRenderState*                  pRenderState) const
{
    BindToCmdBuffer(pCmdBuffer, pRenderState, m_info.graphicsShaderInfos);
}

// =====================================================================================================================
// Binds this graphics pipeline's state to the given command buffer (with passed in wavelimits)
void GraphicsPipeline::BindToCmdBuffer(
    CmdBuffer*                             pCmdBuffer,
    CmdBufferRenderState*                  pRenderState,
    const Pal::DynamicGraphicsShaderInfos& graphicsShaderInfos) const
{
    // Get this pipeline's static tokens
    const auto& newTokens = m_info.staticTokens;

    // Get the old static tokens.  Copy these by value because in MGPU cases we update the new token state in a loop.
    const auto oldTokens = pRenderState->allGpuState.staticTokens;

    // Program static pipeline state.

    // This code will attempt to skip programming state state based on redundant value checks.  These checks are often
    // represented as token compares, where the tokens are two perfect hashes of previously compiled pipelines' static
    // parameter values.
    // If VIEWPORT is static, VIEWPORT_COUNT must be static as well
    if (ContainsStaticState(DynamicStatesInternal::Viewport))
    {
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.viewports, newTokens.viewport))
        {
            pCmdBuffer->SetAllViewports(m_info.viewportParams, newTokens.viewport);
        }
    }
    else if (ContainsStaticState(DynamicStatesInternal::ViewportCount))
    {
        utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
        do
        {
            pRenderState->perGpuState[deviceGroup.Index()].viewport.count = m_info.viewportParams.count;
        }
        while (deviceGroup.IterateNext());

        pRenderState->allGpuState.dirty.viewport = 1;
    }

    if (ContainsStaticState(DynamicStatesInternal::Scissor))
    {
        if (CmdBuffer::IsStaticStateDifferent(oldTokens.scissorRect, newTokens.scissorRect))
        {
            pCmdBuffer->SetAllScissors(m_info.scissorRectParams, newTokens.scissorRect);
        }
    }
    else if (ContainsStaticState(DynamicStatesInternal::ScissorCount))
    {
        utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
        do
        {
            pRenderState->perGpuState[deviceGroup.Index()].scissor.count = m_info.scissorRectParams.count;
        }
        while (deviceGroup.IterateNext());

        pRenderState->allGpuState.dirty.scissor = 1;
    }

    if (m_flags.bindDepthStencilObject == false)
    {
        Pal::DepthStencilStateCreateInfo* pDepthStencilCreateInfo = &(pRenderState->allGpuState.depthStencilCreateInfo);

        if (ContainsStaticState(DynamicStatesInternal::DepthTestEnableExt) &&
            (pDepthStencilCreateInfo->depthEnable != m_info.depthStencilCreateInfo.depthEnable))
        {
            pDepthStencilCreateInfo->depthEnable = m_info.depthStencilCreateInfo.depthEnable;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthWriteEnableExt) &&
            (pDepthStencilCreateInfo->depthWriteEnable != m_info.depthStencilCreateInfo.depthWriteEnable))
        {
            pDepthStencilCreateInfo->depthWriteEnable = m_info.depthStencilCreateInfo.depthWriteEnable;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthCompareOpExt) &&
            (pDepthStencilCreateInfo->depthFunc != m_info.depthStencilCreateInfo.depthFunc))
        {
            pDepthStencilCreateInfo->depthFunc = m_info.depthStencilCreateInfo.depthFunc;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::DepthBoundsTestEnableExt) &&
            (pDepthStencilCreateInfo->depthBoundsEnable != m_info.depthStencilCreateInfo.depthBoundsEnable))
        {
            pDepthStencilCreateInfo->depthBoundsEnable = m_info.depthStencilCreateInfo.depthBoundsEnable;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilTestEnableExt) &&
            (pDepthStencilCreateInfo->stencilEnable != m_info.depthStencilCreateInfo.stencilEnable))
        {
            pDepthStencilCreateInfo->stencilEnable = m_info.depthStencilCreateInfo.stencilEnable;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
        if (ContainsStaticState(DynamicStatesInternal::StencilOpExt) &&
            ((pDepthStencilCreateInfo->front.stencilFailOp != m_info.depthStencilCreateInfo.front.stencilFailOp) ||
             (pDepthStencilCreateInfo->front.stencilPassOp != m_info.depthStencilCreateInfo.front.stencilPassOp) ||
             (pDepthStencilCreateInfo->front.stencilDepthFailOp !=
              m_info.depthStencilCreateInfo.front.stencilDepthFailOp) ||
             (pDepthStencilCreateInfo->front.stencilFunc != m_info.depthStencilCreateInfo.front.stencilFunc) ||
             (pDepthStencilCreateInfo->back.stencilFailOp != m_info.depthStencilCreateInfo.back.stencilFailOp) ||
             (pDepthStencilCreateInfo->back.stencilPassOp != m_info.depthStencilCreateInfo.back.stencilPassOp) ||
             (pDepthStencilCreateInfo->back.stencilDepthFailOp !=
              m_info.depthStencilCreateInfo.back.stencilDepthFailOp) ||
             (pDepthStencilCreateInfo->back.stencilFunc != m_info.depthStencilCreateInfo.back.stencilFunc)))
        {
            pDepthStencilCreateInfo->front = m_info.depthStencilCreateInfo.front;
            pDepthStencilCreateInfo->back  = m_info.depthStencilCreateInfo.back;

            pRenderState->allGpuState.dirty.depthStencil = 1;
        }
    }
    else
    {
        // Update static state to cmdBuffer when it's static DS. It's necessary because
        // it will be used to tell if the value is changed next time.
        pRenderState->allGpuState.depthStencilCreateInfo = m_info.depthStencilCreateInfo;
    }

    if (m_flags.bindTriangleRasterState == false)
    {
        // Update the static states to renderState
        pRenderState->allGpuState.triangleRasterState.frontFillMode   = m_info.triangleRasterState.frontFillMode;
        pRenderState->allGpuState.triangleRasterState.backFillMode    = m_info.triangleRasterState.backFillMode;
        pRenderState->allGpuState.triangleRasterState.provokingVertex = m_info.triangleRasterState.provokingVertex;
        pRenderState->allGpuState.triangleRasterState.flags.u32All    = m_info.triangleRasterState.flags.u32All;

        if (ContainsStaticState(DynamicStatesInternal::FrontFaceExt))
        {
            pRenderState->allGpuState.triangleRasterState.frontFace = m_info.triangleRasterState.frontFace;
        }

        if (ContainsStaticState(DynamicStatesInternal::CullModeExt))
        {
            pRenderState->allGpuState.triangleRasterState.cullMode = m_info.triangleRasterState.cullMode;
        }

        pRenderState->allGpuState.dirty.rasterState = 1;
    }
    else
    {
        pRenderState->allGpuState.triangleRasterState = m_info.triangleRasterState;
    }

    Pal::StencilRefMaskParams prevStencilRefMasks = pRenderState->allGpuState.stencilRefMasks;

    if (m_flags.bindStencilRefMasks == false)
    {
        // Until we expose Stencil Op Value, we always inherit the PSO value, which is currently Default == 1
        pRenderState->allGpuState.stencilRefMasks.frontOpValue   = m_info.stencilRefMasks.frontOpValue;
        pRenderState->allGpuState.stencilRefMasks.backOpValue    = m_info.stencilRefMasks.backOpValue;

        // We don't have to use tokens for these since the combiner does a redundancy check on the full value
        if (ContainsStaticState(DynamicStatesInternal::StencilCompareMask))
        {
            pRenderState->allGpuState.stencilRefMasks.frontReadMask  = m_info.stencilRefMasks.frontReadMask;
            pRenderState->allGpuState.stencilRefMasks.backReadMask   = m_info.stencilRefMasks.backReadMask;
        }

        if (ContainsStaticState(DynamicStatesInternal::StencilWriteMask))
        {
            pRenderState->allGpuState.stencilRefMasks.frontWriteMask = m_info.stencilRefMasks.frontWriteMask;
            pRenderState->allGpuState.stencilRefMasks.backWriteMask  = m_info.stencilRefMasks.backWriteMask;
        }

        if (ContainsStaticState(DynamicStatesInternal::StencilReference))
        {
            pRenderState->allGpuState.stencilRefMasks.frontRef       = m_info.stencilRefMasks.frontRef;
            pRenderState->allGpuState.stencilRefMasks.backRef        = m_info.stencilRefMasks.backRef;
        }
    }
    else
    {
        pRenderState->allGpuState.stencilRefMasks = m_info.stencilRefMasks;
    }

    // Check whether the dirty bit should be set
    if (memcmp(&pRenderState->allGpuState.stencilRefMasks, &prevStencilRefMasks, sizeof(Pal::StencilRefMaskParams)) != 0)
    {
        pRenderState->allGpuState.dirty.stencilRef = 1;
    }

    if (m_flags.bindInputAssemblyState == false)
    {
        // Update the static states to renderState
        pRenderState->allGpuState.inputAssemblyState.primitiveRestartIndex =
            m_info.inputAssemblyState.primitiveRestartIndex;

        pRenderState->allGpuState.inputAssemblyState.primitiveRestartEnable =
            m_info.inputAssemblyState.primitiveRestartEnable;

        pRenderState->allGpuState.dirty.inputAssembly = 1;
    }
    else
    {
        pRenderState->allGpuState.inputAssemblyState = m_info.inputAssemblyState;
    }

    utils::IterateMask deviceGroup(pCmdBuffer->GetDeviceMask());
    do
    {
        const uint32_t deviceIdx = deviceGroup.Index();

        Pal::ICmdBuffer* pPalCmdBuf = pCmdBuffer->PalCmdBuffer(deviceIdx);

        if (pRenderState->allGpuState.pGraphicsPipeline != nullptr)
        {
            const uint64_t oldHash = pRenderState->allGpuState.pGraphicsPipeline->PalPipelineHash();
            const uint64_t newHash = PalPipelineHash();

            if ((oldHash != newHash)
                )
            {
                Pal::PipelineBindParams params = {};
                params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
                params.pPipeline         = m_pPalPipeline[deviceIdx];
                params.graphics          = graphicsShaderInfos;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
                params.apiPsoHash = m_apiHash;
#endif

                pPalCmdBuf->CmdBindPipeline(params);
            }
        }
        else
        {
            Pal::PipelineBindParams params = {};
            params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
            params.pPipeline         = m_pPalPipeline[deviceIdx];
            params.graphics          = graphicsShaderInfos;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
            params.apiPsoHash = m_apiHash;
#endif

            pPalCmdBuf->CmdBindPipeline(params);
        }

        // Bind state objects that are always static; these are redundancy checked by the pointer in the command buffer.
        if (m_flags.bindDepthStencilObject)
        {
            pCmdBuffer->PalCmdBindDepthStencilState(pPalCmdBuf, deviceIdx, m_pPalDepthStencil[deviceIdx]);

            pRenderState->allGpuState.dirty.depthStencil = 0;
        }

        pCmdBuffer->PalCmdBindColorBlendState(pPalCmdBuf, deviceIdx, m_pPalColorBlend[deviceIdx]);
        pCmdBuffer->PalCmdBindMsaaState(pPalCmdBuf, deviceIdx, m_pPalMsaa[deviceIdx]);

        // Write parameters that are marked static pipeline state.  Redundancy check these based on static tokens:
        // skip the write if the previously written static token matches.

        if (CmdBuffer::IsStaticStateDifferent(oldTokens.inputAssemblyState, newTokens.inputAssemblyState) &&
                m_flags.bindInputAssemblyState)
        {
            pPalCmdBuf->CmdSetInputAssemblyState(m_info.inputAssemblyState);

            pRenderState->allGpuState.staticTokens.inputAssemblyState = newTokens.inputAssemblyState;
            pRenderState->allGpuState.dirty.inputAssembly             = 0;
        }

        if (CmdBuffer::IsStaticStateDifferent(oldTokens.triangleRasterState, newTokens.triangleRasterState) &&
                m_flags.bindTriangleRasterState)
        {
            pPalCmdBuf->CmdSetTriangleRasterState(m_info.triangleRasterState);

            pRenderState->allGpuState.staticTokens.triangleRasterState = newTokens.triangleRasterState;
            pRenderState->allGpuState.dirty.rasterState                = 0;
        }

        if (ContainsStaticState(DynamicStatesInternal::LineWidth) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.pointLineRasterState, newTokens.pointLineRasterState))
        {
            pPalCmdBuf->CmdSetPointLineRasterState(m_info.pointLineRasterParams);
            pRenderState->allGpuState.staticTokens.pointLineRasterState = newTokens.pointLineRasterState;
        }

        if (ContainsStaticState(DynamicStatesInternal::LineStippleExt) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.lineStippleState, newTokens.lineStippleState))
        {
            pPalCmdBuf->CmdSetLineStippleState(m_info.lineStippleParams);
            pRenderState->allGpuState.staticTokens.lineStippleState = newTokens.lineStippleState;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBias) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBiasState, newTokens.depthBias))
        {
            pPalCmdBuf->CmdSetDepthBiasState(m_info.depthBiasParams);
            pRenderState->allGpuState.staticTokens.depthBiasState = newTokens.depthBias;
        }

        if (ContainsStaticState(DynamicStatesInternal::BlendConstants) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.blendConst, newTokens.blendConst))
        {
            pPalCmdBuf->CmdSetBlendConst(m_info.blendConstParams);
            pRenderState->allGpuState.staticTokens.blendConst = newTokens.blendConst;
        }

        if (ContainsStaticState(DynamicStatesInternal::DepthBounds) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.depthBounds, newTokens.depthBounds))
        {
            pPalCmdBuf->CmdSetDepthBounds(m_info.depthBoundParams);
            pRenderState->allGpuState.staticTokens.depthBounds = newTokens.depthBounds;
        }

        if (ContainsStaticState(DynamicStatesInternal::SampleLocationsExt) &&
            CmdBuffer::IsStaticStateDifferent(oldTokens.samplePattern, newTokens.samplePattern))
        {
            pCmdBuffer->PalCmdSetMsaaQuadSamplePattern(
                m_info.samplePattern.sampleCount, m_info.samplePattern.locations);
            pRenderState->allGpuState.staticTokens.samplePattern = newTokens.samplePattern;
        }
    }
    while (deviceGroup.IterateNext());

    // Binding GraphicsPipeline affects ViewMask,
    // because when VK_PIPELINE_CREATE_VIEW_INDEX_FROM_DEVICE_INDEX_BIT is specified
    // ViewMask for each VkPhysicalDevice is defined by DeviceIndex
    // not by current subpass during a render pass instance.
    const bool oldViewIndexFromDeviceIndex = pRenderState->allGpuState.viewIndexFromDeviceIndex;
    const bool newViewIndexFromDeviceIndex = ViewIndexFromDeviceIndex();

    if (oldViewIndexFromDeviceIndex != newViewIndexFromDeviceIndex)
    {
        // Update value of ViewIndexFromDeviceIndex for currently bound pipeline.
        pRenderState->allGpuState.viewIndexFromDeviceIndex = newViewIndexFromDeviceIndex;

        // Sync ViewMask state in CommandBuffer.
        pCmdBuffer->SetViewInstanceMask(pCmdBuffer->GetDeviceMask());
    }
}

// =====================================================================================================================
// Binds a null pipeline to PAL
void GraphicsPipeline::BindNullPipeline(CmdBuffer* pCmdBuffer)
{
    const uint32_t numDevices = pCmdBuffer->VkDevice()->NumPalDevices();

    Pal::PipelineBindParams params = {};
    params.pipelineBindPoint = Pal::PipelineBindPoint::Graphics;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 471
    params.apiPsoHash = Pal::InternalApiPsoHash;
#endif

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
