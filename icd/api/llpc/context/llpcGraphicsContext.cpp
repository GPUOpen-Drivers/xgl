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
 * @file  llpcGraphicsContext.cpp
 * @brief LLPC source file: contains implementation of class Llpc::GraphicsContext.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-graphics-context"

#include "SPIRVInternal.h"
#include "llpcCompiler.h"
#include "llpcGfx6Chip.h"
#include "llpcGfx9Chip.h"
#include "llpcGraphicsContext.h"

#include "llpcInternal.h"

using namespace llvm;
using namespace SPIRV;

namespace llvm
{

namespace cl
{

// -enable-tess-offchip: enable tessellation off-chip mode
opt<bool> EnableTessOffChip("enable-tess-offchip",
                            desc("Enable tessellation off-chip mode "),
                            init(false));

// -disable-gs-onchip: disable geometry shader on-chip mode
opt<bool> DisableGsOnChip("disable-gs-onchip",
                          desc("Disable geometry shader on-chip mode"),
                          init(false));

} // cl

} // llvm

namespace Llpc
{

// =====================================================================================================================
GraphicsContext::GraphicsContext(
    GfxIpVersion                     gfxIp,         // Graphics Ip version info
    const GpuProperty*               pGpuProp,      // GPU Property
    const GraphicsPipelineBuildInfo* pPipelineInfo, // [in] Graphics pipeline build info
    MetroHash::Hash*                 pHash)         // [in] Pipeline hash code
    :
    PipelineContext(gfxIp, pGpuProp, pHash),
    m_pPipelineInfo(pPipelineInfo),
    m_stageMask(0),
    m_activeStageCount(0),
    m_tessOffchip(cl::EnableTessOffChip),
    m_gsOnChip(false)
{
    if (gfxIp.major >= 9)
    {
        // For GFX9+, always enable tessellation off-chip mode
        m_tessOffchip = true;
    }

    const PipelineShaderInfo* shaderInfo[ShaderStageGfxCount] =
    {
        &pPipelineInfo->vs,
        &pPipelineInfo->tcs,
        &pPipelineInfo->tes,
        &pPipelineInfo->gs,
        &pPipelineInfo->fs,
    };

    for (uint32_t stage = 0; stage < ShaderStageGfxCount; ++stage)
    {
        if (shaderInfo[stage]->pModuleData != nullptr)
        {
            m_stageMask |= ShaderStageToMask(static_cast<ShaderStage>(stage));
            ++m_activeStageCount;

            if (stage == ShaderStageGeometry)
            {
                m_stageMask |= ShaderStageToMask(ShaderStageCopyShader);
                ++m_activeStageCount;
            }
        }
    }

    for (uint32_t stage = 0; stage < ShaderStageGfxCount; ++stage)
    {
        InitShaderResourceUsage(static_cast<ShaderStage>(stage));
        InitShaderInterfaceData(static_cast<ShaderStage>(stage));
    }

    memset(&m_dummyVertexInput, 0, sizeof(m_dummyVertexInput));
}

// =====================================================================================================================
GraphicsContext::~GraphicsContext()
{
    for (auto pAllocNodes : m_allocUserDataNodes)
    {
        delete pAllocNodes;
    }
}

// =====================================================================================================================
// Gets resource usage of the specified shader stage.
ResourceUsage* GraphicsContext::GetShaderResourceUsage(
    ShaderStage shaderStage) // Shader stage
{
    if (shaderStage == ShaderStageCopyShader)
    {
        // Treat copy shader as part of geometry shader
        shaderStage = ShaderStageGeometry;
    }

    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);
    return &m_resUsages[shaderStage];
}

// =====================================================================================================================
// Gets interface data of the specified shader stage.
InterfaceData* GraphicsContext::GetShaderInterfaceData(
    ShaderStage shaderStage)  // Shader stage
{
    if (shaderStage == ShaderStageCopyShader)
    {
        // Treat copy shader as part of geometry shader
        shaderStage = ShaderStageGeometry;
    }

    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);
    return &m_intfData[shaderStage];
}

// =====================================================================================================================
// Gets pipeline shader info of the specified shader stage
const PipelineShaderInfo* GraphicsContext::GetPipelineShaderInfo(
    ShaderStage shaderStage // Shader stage
    ) const
{
    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);

    const PipelineShaderInfo* pShaderInfo = nullptr;
    switch (shaderStage)
    {
    case Llpc::ShaderStageVertex:
        pShaderInfo = &m_pPipelineInfo->vs;
        break;
    case Llpc::ShaderStageTessControl:
        pShaderInfo = &m_pPipelineInfo->tcs;
        break;
    case Llpc::ShaderStageTessEval:
        pShaderInfo = &m_pPipelineInfo->tes;
        break;
    case Llpc::ShaderStageGeometry:
        pShaderInfo = &m_pPipelineInfo->gs;
        break;
    case Llpc::ShaderStageFragment:
        pShaderInfo = &m_pPipelineInfo->fs;
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }

    return pShaderInfo;
}

// =====================================================================================================================
// Gets the previous active shader stage in this pipeline
ShaderStage GraphicsContext::GetPrevShaderStage(
    ShaderStage shaderStage // Current shader stage
    ) const
{
    if (shaderStage == ShaderStageCopyShader)
    {
        // Treat copy shader as part of geometry shader
        shaderStage = ShaderStageGeometry;
    }

    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);

    ShaderStage prevStage = ShaderStageInvalid;

    for (int32_t stage = shaderStage - 1; stage >= 0; --stage)
    {
        if ((m_stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage))) != 0)
        {
            prevStage = static_cast<ShaderStage>(stage);
            break;
        }
    }

    return prevStage;
}

// =====================================================================================================================
// Gets the previous active shader stage in this pipeline
ShaderStage GraphicsContext::GetNextShaderStage(
    ShaderStage shaderStage // Current shader stage
    ) const
{
    if (shaderStage == ShaderStageCopyShader)
    {
        // Treat copy shader as part of geometry shader
        shaderStage = ShaderStageGeometry;
    }

    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);

    ShaderStage nextStage = ShaderStageInvalid;

    for (uint32_t stage = shaderStage + 1; stage < ShaderStageGfxCount; ++stage)
    {
        if ((m_stageMask & ShaderStageToMask(static_cast<ShaderStage>(stage))) != 0)
        {
            nextStage = static_cast<ShaderStage>(stage);
            break;
        }
    }

    return nextStage;
}

// =====================================================================================================================
// Gets dummy resource mapping nodes of the specified shader stage.
std::vector<ResourceMappingNode>* GraphicsContext::GetDummyResourceMapNodes(
  ShaderStage shaderStage)  // Shader stage
{
    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);
    return &m_dummyResMapNodes[shaderStage];
}

// =====================================================================================================================
// Initializes shader info for null fragment shader.
void GraphicsContext::InitShaderInfoForNullFs()
{
    auto pResUsage = GetShaderResourceUsage(ShaderStageFragment);
    m_stageMask |= ShaderStageToMask(ShaderStageFragment);
    ++m_activeStageCount;

    // Add usage info for dummy input
    FsInterpInfo interpInfo = { 0, false, false, false };
    pResUsage->builtInUsage.fs.smooth = true;
    pResUsage->inOutUsage.inputLocMap[0] = InvalidValue;
    pResUsage->inOutUsage.fs.interpInfo.push_back(interpInfo);

    // Add usage info for dummy output
    pResUsage->inOutUsage.fs.cbShaderMask = 0xF;
    pResUsage->inOutUsage.outputLocMap[0] = InvalidValue;
}

// =====================================================================================================================
// Gets the hash code of input shader with specified shader stage.
//
// NOTE: This function must be same with BilManager::GenerateShaderHashCode
uint64_t GraphicsContext::GetShaderHashCode(
    ShaderStage shaderStage     // Shader stage
    ) const
{
    LLPC_ASSERT(shaderStage < ShaderStageGfxCount);

    uint64_t shaderHash = 0;
    auto pShaderInfo = GetPipelineShaderInfo(shaderStage);

    if (pShaderInfo->pModuleData != nullptr)
    {
        MetroHash64 hasher;

        UpdateShaderHashForPipelineShaderInfo(shaderStage, pShaderInfo, &hasher);
        hasher.Update(m_pPipelineInfo->iaState.deviceIndex);

        if (shaderStage == ShaderStageTessControl)
        {
            hasher.Update(m_pPipelineInfo->iaState.patchControlPoints);
        }
        else if ((shaderStage == ShaderStageVertex) &&
                 (m_pPipelineInfo->pVertexInput != nullptr) &&
                 (m_pPipelineInfo->pVertexInput->vertexBindingDescriptionCount > 0) &&
                 (m_pPipelineInfo->pVertexInput->vertexAttributeDescriptionCount > 0))
        {
            auto pVertexInput = m_pPipelineInfo->pVertexInput;
            hasher.Update(pVertexInput->vertexBindingDescriptionCount);
            hasher.Update(reinterpret_cast<const uint8_t*>(pVertexInput->pVertexBindingDescriptions),
                          sizeof(VkVertexInputBindingDescription) * pVertexInput->vertexBindingDescriptionCount);
            hasher.Update(pVertexInput->vertexAttributeDescriptionCount);
            hasher.Update(reinterpret_cast<const uint8_t*>(pVertexInput->pVertexAttributeDescriptions),
                          sizeof(VkVertexInputAttributeDescription) * pVertexInput->vertexAttributeDescriptionCount);
        }
        else if (shaderStage == ShaderStageFragment)
        {
            if (m_pPipelineInfo->rsState.perSampleShading)
            {
                hasher.Update(m_pPipelineInfo->rsState.perSampleShading);
            }
        }

        MetroHash::Hash hash = {};
        hasher.Finalize(hash.bytes);

        shaderHash = MetroHash::Compact64(&hash);
    }

    return shaderHash;
}

// =====================================================================================================================
// Determines whether GS on-chip mode is valid for this pipeline, also computes ES-GS/GS-VS ring item size.
bool GraphicsContext::CheckGsOnChipValidity()
{
    bool gsOnChip = true;

    uint32_t stageMask = GetShaderStageMask();
    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                        ShaderStageToMask(ShaderStageTessEval))) != 0);

    auto pEsResUsage = GetShaderResourceUsage(hasTs ? ShaderStageTessEval : ShaderStageVertex);
    auto pGsResUsage = GetShaderResourceUsage(ShaderStageGeometry);

    uint32_t vertsPerPrim = 1;
    bool     useAdjacency = false;
    switch (pGsResUsage->builtInUsage.gs.inputPrimitive)
    {
    case InputPoints:
        vertsPerPrim = 1;
        break;
    case InputLines:
        vertsPerPrim = 2;
        break;
    case InputLinesAdjacency:
        useAdjacency = true;
        vertsPerPrim = 4;
        break;
    case InputTriangles:
        vertsPerPrim = 3;
        break;
    case InputTrianglesAdjacency:
        useAdjacency = true;
        vertsPerPrim = 6;
        break;
    default:
        LLPC_NEVER_CALLED();
        break;
    }

    pGsResUsage->inOutUsage.gs.calcFactor.inputVertices = vertsPerPrim;

    if (m_gfxIp.major <= 8)
    {
        uint32_t gsPrimsPerSubgroup = m_pGpuProperty->gsOnChipDefaultPrimsPerSubgroup;

        const uint32_t esGsRingItemSize = 4 * std::max(1u, pEsResUsage->inOutUsage.outputMapLocCount);
        const uint32_t gsInstanceCount  = pGsResUsage->builtInUsage.gs.invocations;
        const uint32_t gsVsRingItemSize = 4 * std::max(1u,
                                                       (pGsResUsage->inOutUsage.outputMapLocCount *
                                                        pGsResUsage->builtInUsage.gs.outputVertices));

        uint32_t esGsRingItemSizeOnChip = esGsRingItemSize;
        uint32_t gsVsRingItemSizeOnChip = gsVsRingItemSize;

        // optimize ES -> GS ring and GS -> VS ring layout for bank conflicts
        esGsRingItemSizeOnChip |= 1;
        gsVsRingItemSizeOnChip |= 1;

        uint32_t gsVsRingItemSizeOnChipInstanced = gsVsRingItemSizeOnChip * gsInstanceCount;

        uint32_t esMinVertsPerSubgroup = vertsPerPrim;

        // If the primitive has adjacency half the number of vertices will be reused in multiple primitives.
        if (useAdjacency)
        {
            esMinVertsPerSubgroup >>= 1;
        }

        // There is a hardware requirement for gsPrimsPerSubgroup * gsInstanceCount to be capped by GsOnChipMaxPrimsPerSubgroup
        // for adjacency primitive or when GS instanceing is used.
        if (useAdjacency || (gsInstanceCount > 1))
        {
            gsPrimsPerSubgroup = std::min(gsPrimsPerSubgroup, (Gfx6::GsOnChipMaxPrimsPerSubgroup / gsInstanceCount));
        }

        // Compute GS-VS LDS size based on target GS primitives per subgroup
        uint32_t gsVsLdsSize = (gsVsRingItemSizeOnChipInstanced * gsPrimsPerSubgroup);

        // Compute ES-GS LDS size based on the worst case number of ES vertices needed to create the target number of
        // GS primitives per subgroup.
        uint32_t esGsLdsSize = esGsRingItemSizeOnChip * esMinVertsPerSubgroup * gsPrimsPerSubgroup;

        // Total LDS use per subgroup aligned to the register granularity
        uint32_t gsOnChipLdsSize = Pow2Align((esGsLdsSize + gsVsLdsSize),
                                             static_cast<uint32_t>((1 << m_pGpuProperty->ldsSizeDwordGranularityShift)));

        // Use the client-specified amount of LDS space per subgroup. If they specified zero, they want us to choose a
        // reasonable default. The final amount must be 128-DWORD aligned.

        uint32_t maxLdsSize = m_pGpuProperty->gsOnChipDefaultLdsSizePerSubgroup;

        // TODO: For BONAIRE A0, GODAVARI and KALINDI, set maxLdsSize to 1024 due to SPI barrier management bug

        // If total LDS usage is too big, refactor partitions based on ratio of ES-GS and GS-VS item sizes.
        if (gsOnChipLdsSize > maxLdsSize)
        {
            const uint32_t esGsItemSizePerPrim = esGsRingItemSizeOnChip * esMinVertsPerSubgroup;
            const uint32_t itemSizeTotal       = esGsItemSizePerPrim + gsVsRingItemSizeOnChipInstanced;

            esGsLdsSize = RoundUpToMultiple((esGsItemSizePerPrim * maxLdsSize) / itemSizeTotal, esGsItemSizePerPrim);
            gsVsLdsSize = RoundDownToMultiple(maxLdsSize - esGsLdsSize, gsVsRingItemSizeOnChipInstanced);

            gsOnChipLdsSize = maxLdsSize;
        }

        // Based on the LDS space, calculate how many GS prims per subgroup and ES vertices per subgroup can be dispatched.
        gsPrimsPerSubgroup          = (gsVsLdsSize / gsVsRingItemSizeOnChipInstanced);
        uint32_t esVertsPerSubgroup = (esGsLdsSize / esGsRingItemSizeOnChip);

        LLPC_ASSERT(esVertsPerSubgroup >= esMinVertsPerSubgroup);

        // Vertices for adjacency primitives are not always reused. According to
        // hardware engineers, we must restore esMinVertsPerSubgroup for ES_VERTS_PER_SUBGRP.
        if (useAdjacency)
        {
            esMinVertsPerSubgroup = vertsPerPrim;
        }

        // For normal primitives, the VGT only checks if they are past the ES verts per sub-group after allocating a full
        // GS primitive and if they are, kick off a new sub group. But if those additional ES vertices are unique
        // (e.g. not reused) we need to make sure there is enough LDS space to account for those ES verts beyond
        // ES_VERTS_PER_SUBGRP.
        esVertsPerSubgroup -= (esMinVertsPerSubgroup - 1);

        // TODO: Accept GsOffChipDefaultThreshold from panel option
        // TODO: Value of GsOffChipDefaultThreshold should be 64, due to an issue it's changed to 32 in order to test
        // on-chip GS code generation before fixing that issue.
        // The issue is because we only remove unused builtin output till final GS output store generation, when
        // determining onchip/offchip mode, unused builtin output like PointSize and Clip/CullDistance is factored in
        // LDS usage and deactivates onchip GS when GsOffChipDefaultThreshold  is 64. To fix this we will probably
        // need to clear unused builtin ouput before determining onchip/offchip GS mode.
        constexpr uint32_t GsOffChipDefaultThreshold = 32;

        bool disableGsOnChip = cl::DisableGsOnChip;
        if (hasTs || (m_gfxIp.major == 6))
        {
            // GS on-chip is not supportd with tessellation, and is not supportd on GFX6
            disableGsOnChip = true;
        }

        if (disableGsOnChip ||
            ((gsPrimsPerSubgroup * gsInstanceCount) < GsOffChipDefaultThreshold) ||
            (esVertsPerSubgroup == 0))
        {
            gsOnChip = false;
            pGsResUsage->inOutUsage.gs.calcFactor.esVertsPerSubgroup   = 0;
            pGsResUsage->inOutUsage.gs.calcFactor.gsPrimsPerSubgroup   = 0;
            pGsResUsage->inOutUsage.gs.calcFactor.esGsLdsSize          = 0;
            pGsResUsage->inOutUsage.gs.calcFactor.gsOnChipLdsSize      = 0;

            pGsResUsage->inOutUsage.gs.calcFactor.esGsRingItemSize     = esGsRingItemSize;
            pGsResUsage->inOutUsage.gs.calcFactor.gsVsRingItemSize     = gsVsRingItemSize;
        }
        else
        {
            pGsResUsage->inOutUsage.gs.calcFactor.esVertsPerSubgroup   = esVertsPerSubgroup;
            pGsResUsage->inOutUsage.gs.calcFactor.gsPrimsPerSubgroup   = gsPrimsPerSubgroup;
            pGsResUsage->inOutUsage.gs.calcFactor.esGsLdsSize          = esGsLdsSize;
            pGsResUsage->inOutUsage.gs.calcFactor.gsOnChipLdsSize      = gsOnChipLdsSize;

            pGsResUsage->inOutUsage.gs.calcFactor.esGsRingItemSize     = esGsRingItemSizeOnChip;
            pGsResUsage->inOutUsage.gs.calcFactor.gsVsRingItemSize     = gsVsRingItemSizeOnChip;
        }
    }
    else
    {
        uint32_t gsPrimsPerSubgroup = m_pGpuProperty->gsOnChipDefaultPrimsPerSubgroup;

        // NOTE: Make esGsItemSize odd by "| 1", to optimize ES -> GS ring layout for LDS bank conflicts
        const uint32_t esGsItemSize     = (4 * std::max(1u, pEsResUsage->inOutUsage.outputMapLocCount)) | 1;
        const uint32_t gsVsRingItemSize = 4 * std::max(1u,
                                                       (pGsResUsage->inOutUsage.outputMapLocCount *
                                                        pGsResUsage->builtInUsage.gs.outputVertices));
        const uint32_t gsInstanceCount  = pGsResUsage->builtInUsage.gs.invocations;
        // TODO: Confirm no extra LDS space used in ES and GS
        const uint32_t esGsExtraLdsDwords  = 0;
        const uint32_t maxEsVertsPerSubgroup = Gfx9::OnChipGsMaxEsVertsPerSubgroup;

        uint32_t esMinVertsPerSubgroup = vertsPerPrim;

        // If the primitive has adjacency half the number of vertices will be reused in multiple primitives.
        if (useAdjacency)
        {
            esMinVertsPerSubgroup >>= 1;
        }

        uint32_t maxGsPrimsPerSubgroup = Gfx9::OnChipGsMaxPrimPerSubgroup;

        // There is a hardware requirement for gsPrimsPerSubgroup * gsInstanceCount to be capped by
        // OnChipGsMaxPrimPerSubgroup for adjacency primitive or when GS instanceing is used.
        if (useAdjacency || (gsInstanceCount > 1))
        {
            maxGsPrimsPerSubgroup = (Gfx9::OnChipGsMaxPrimPerSubgroupAdj / gsInstanceCount);
        }

        gsPrimsPerSubgroup = std::min(gsPrimsPerSubgroup, maxGsPrimsPerSubgroup);

        uint32_t worstCaseEsVertsPerSubgroup = std::min(esMinVertsPerSubgroup * gsPrimsPerSubgroup,
                                                        maxEsVertsPerSubgroup);

        uint32_t esGsLdsSize = (esGsItemSize * worstCaseEsVertsPerSubgroup);

        // Total LDS use per subgroup aligned to the register granularity.
        uint32_t gsOnChipLdsSize =
            RoundUpToMultiple(esGsLdsSize + esGsExtraLdsDwords,
                              static_cast<uint32_t>(1 << m_pGpuProperty->ldsSizeDwordGranularityShift));

        // Use the client-specified amount of LDS space per sub-group. If they specified zero, they want us to choose a
        // reasonable default. The final amount must be 128-DWORD aligned.
        // TODO: Accept DefaultLdsSizePerSubGroup from panel setting
        uint32_t maxLdsSize = Gfx9::DefaultLdsSizePerSubGroup;

        // If total LDS usage is too big, refactor partitions based on ratio of ES-GS item sizes.
        if (gsOnChipLdsSize > maxLdsSize)
        {
            // Our target GS primitives per sub-group was too large

            // Calculate the maximum number of GS primitives per sub-group that will fit into LDS, capped
            // by the maximum that the hardware can support.
            uint32_t availableLdsSize   = maxLdsSize - esGsExtraLdsDwords;
            gsPrimsPerSubgroup          = std::min((availableLdsSize / (esGsItemSize * esMinVertsPerSubgroup)),
                                                   maxGsPrimsPerSubgroup);
            worstCaseEsVertsPerSubgroup = std::min(esMinVertsPerSubgroup * gsPrimsPerSubgroup, maxEsVertsPerSubgroup);

            LLPC_ASSERT(gsPrimsPerSubgroup > 0);

            esGsLdsSize     = (esGsItemSize * worstCaseEsVertsPerSubgroup);
            gsOnChipLdsSize =
                RoundUpToMultiple(esGsLdsSize + esGsExtraLdsDwords,
                                  1u << static_cast<uint32_t>(1 << m_pGpuProperty->ldsSizeDwordGranularityShift));
            LLPC_ASSERT(gsOnChipLdsSize <= maxLdsSize);
        }

        // TODO: Check GS -> VS ring on-chip validity

        uint32_t esVertsPerSubgroup = std::min(esGsLdsSize / esGsItemSize, maxEsVertsPerSubgroup);

        LLPC_ASSERT(esVertsPerSubgroup >= esMinVertsPerSubgroup);

        // Vertices for adjacency primitives are not always reused (e.g. in the case of shadow volumes). Acording to
        // hardware engineers, we must restore esMinVertsPerSubgroup for ES_VERTS_PER_SUBGRP.
        if (useAdjacency)
        {
            esMinVertsPerSubgroup = vertsPerPrim;
        }

        // For normal primitives, the VGT only checks if they are past the ES verts per sub group after allocating
        // a full GS primitive and if they are, kick off a new sub group.  But if those additional ES verts are
        // unique (e.g. not reused) we need to make sure there is enough LDS space to account for those ES verts
        // beyond ES_VERTS_PER_SUBGRP.
        esVertsPerSubgroup -= (esMinVertsPerSubgroup - 1);

        pGsResUsage->inOutUsage.gs.calcFactor.esVertsPerSubgroup   = esVertsPerSubgroup;
        pGsResUsage->inOutUsage.gs.calcFactor.gsPrimsPerSubgroup   = gsPrimsPerSubgroup;
        pGsResUsage->inOutUsage.gs.calcFactor.esGsLdsSize          = esGsLdsSize;
        pGsResUsage->inOutUsage.gs.calcFactor.gsOnChipLdsSize      = gsOnChipLdsSize;

        pGsResUsage->inOutUsage.gs.calcFactor.esGsRingItemSize     = esGsItemSize;
        pGsResUsage->inOutUsage.gs.calcFactor.gsVsRingItemSize     = gsVsRingItemSize;

        // TODO: GFX9 GS -> VS ring on chip is not supported yet
        gsOnChip = false;
    }

    LLPC_OUTS("===============================================================================\n");
    LLPC_OUTS("// LLPC geometry calculation factor results\n\n");
    LLPC_OUTS("ES vertices per sub-group: " << pGsResUsage->inOutUsage.gs.calcFactor.esVertsPerSubgroup << "\n");
    LLPC_OUTS("GS primitives per sub-group: " << pGsResUsage->inOutUsage.gs.calcFactor.gsPrimsPerSubgroup << "\n");
    LLPC_OUTS("\n");
    LLPC_OUTS("ES-GS LDS size: " << pGsResUsage->inOutUsage.gs.calcFactor.esGsLdsSize << "\n");
    LLPC_OUTS("On-chip GS LDS size: " << pGsResUsage->inOutUsage.gs.calcFactor.gsOnChipLdsSize << "\n");
    LLPC_OUTS("\n");
    LLPC_OUTS("ES-GS ring item size: " << pGsResUsage->inOutUsage.gs.calcFactor.esGsRingItemSize << "\n");
    LLPC_OUTS("GS-VS ring item size: " << pGsResUsage->inOutUsage.gs.calcFactor.gsVsRingItemSize << "\n");
    LLPC_OUTS("\n");
    if (gsOnChip || (m_gfxIp.major >= 9))
    {
        LLPC_OUTS("GS is on-chip\n");
    }
    else
    {
        LLPC_OUTS("GS is off-chip\n");
    }
    LLPC_OUTS("\n");

    return gsOnChip;
}

// =====================================================================================================================
// Does user data node merging for merged shader
void GraphicsContext::DoUserDataNodeMerge()
{
    const bool hasVs  = ((m_stageMask & ShaderStageToMask(ShaderStageVertex)) != 0);
    const bool hasTcs = ((m_stageMask & ShaderStageToMask(ShaderStageTessControl)) != 0);
    const bool hasTes = ((m_stageMask & ShaderStageToMask(ShaderStageTessEval)) != 0);
    const bool hasGs  = ((m_stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    const bool hasTs = (hasTcs || hasTes);

    // Merge user data nodes only if tessellation shader or geometry shader is present
    if (hasTs || hasGs)
    {
        // Merge user data nodes for LS-HS merged shader
        if (hasVs && hasTcs)
        {
            uint32_t mergedNodeCount = 0;
            const ResourceMappingNode* pMergedNodes = nullptr;

            auto pShaderInfo1 = const_cast<PipelineShaderInfo*>(&m_pPipelineInfo->vs);
            auto pShaderInfo2 = const_cast<PipelineShaderInfo*>(&m_pPipelineInfo->tcs);

            MergeUserDataNode(pShaderInfo1->userDataNodeCount,
                              pShaderInfo1->pUserDataNodes,
                              pShaderInfo2->userDataNodeCount,
                              pShaderInfo2->pUserDataNodes,
                              &mergedNodeCount,
                              &pMergedNodes);

            pShaderInfo1->userDataNodeCount = mergedNodeCount;
            pShaderInfo1->pUserDataNodes    = pMergedNodes;
            pShaderInfo2->userDataNodeCount = mergedNodeCount;
            pShaderInfo2->pUserDataNodes    = pMergedNodes;
        }

        // Merge user data nodes for ES-GS merged shader
        if (((hasTs && hasTes) || ((hasTs == false) && hasVs)) && hasGs)
        {
            uint32_t mergedNodeCount = 0;
            const ResourceMappingNode* pMergedNodes = nullptr;

            auto pShaderInfo1 = hasTs ? const_cast<PipelineShaderInfo*>(&m_pPipelineInfo->tes) :
                                        const_cast<PipelineShaderInfo*>(&m_pPipelineInfo->vs);
            auto pShaderInfo2 = const_cast<PipelineShaderInfo*>(&m_pPipelineInfo->gs);

            MergeUserDataNode(pShaderInfo1->userDataNodeCount,
                              pShaderInfo1->pUserDataNodes,
                              pShaderInfo2->userDataNodeCount,
                              pShaderInfo2->pUserDataNodes,
                              &mergedNodeCount,
                              &pMergedNodes);

            pShaderInfo1->userDataNodeCount = mergedNodeCount;
            pShaderInfo1->pUserDataNodes    = pMergedNodes;
            pShaderInfo2->userDataNodeCount = mergedNodeCount;
            pShaderInfo2->pUserDataNodes    = pMergedNodes;
        }
    }
}

// =====================================================================================================================
// Compare the offsets of the two user data node (use "less" relational operator).
static inline bool CompareNode(
    const ResourceMappingNode* pNodes1, // [in] The first user data node
    const ResourceMappingNode* pNodes2) // [in] The second user data node
{
    return (pNodes1->offsetInDwords < pNodes2->offsetInDwords);
}

// =====================================================================================================================
// Merges user data nodes for LS-HS/ES-GS merged shader.
//
// NOTE: We assume those user data nodes are sorted in asceding order of the DWORD offset.
void GraphicsContext::MergeUserDataNode(
    uint32_t                    nodeCount1,         // Count of user data nodes of the first shader
    const ResourceMappingNode*  pNodes1,            // [in] User data nodes of the firset shader
    uint32_t                    nodeCount2,         // Count of user data nodes of the second shader
    const ResourceMappingNode*  pNodes2,            // [in] User data nodes of the second shader
    uint32_t*                   pMergedNodeCount,   // [out] Count of user data nodes of the merged shader
    const ResourceMappingNode** ppMergedNodes)      // [out] User data nodes of the merged shader
{
    uint32_t mergedNodeCount = 0;
    ResourceMappingNode* pMergedNodes = nullptr;

    if ((nodeCount1 > 0) && (nodeCount2 > 0))
    {
        // Sort the two lists
        std::vector<const ResourceMappingNode*> nodes1;
        std::vector<const ResourceMappingNode*> nodes2;

        for (uint32_t i = 0; i < nodeCount1; ++i)
        {
            nodes1.push_back(&pNodes1[i]);
        }

        for (uint32_t i = 0; i < nodeCount2; ++i)
        {
            nodes2.push_back(&pNodes2[i]);
        }

        std::sort(nodes1.begin(), nodes1.end(), CompareNode);
        std::sort(nodes2.begin(), nodes2.end(), CompareNode);

        // Merge the two lists
        std::vector<ResourceMappingNode> mergedNodes;

        uint32_t nodeOffset = 0;

        uint32_t nodeIdx1 = 0;
        uint32_t nodeIdx2 = 0;

        // Visit the two lists until one of them is finished
        while ((nodeIdx1 < nodeCount1) && (nodeIdx2 < nodeCount2))
        {
            const ResourceMappingNode* pNode1 = nodes1[nodeIdx1];
            const ResourceMappingNode* pNode2 = nodes2[nodeIdx2];

            ResourceMappingNode mergedNode = {};

            if (pNode1->offsetInDwords < pNode2->offsetInDwords)
            {
                LLPC_ASSERT(pNode1->offsetInDwords >= nodeOffset);
                LLPC_ASSERT(pNode1->offsetInDwords + pNode1->sizeInDwords <= pNode2->offsetInDwords); // Not overlapped
                mergedNode = *pNode1;

                nodeOffset = pNode1->offsetInDwords + pNode1->sizeInDwords;
                ++nodeIdx1;
            }
            else if (pNode2->offsetInDwords < pNode1->offsetInDwords)
            {
                LLPC_ASSERT(pNode2->offsetInDwords >= nodeOffset);
                LLPC_ASSERT(pNode2->offsetInDwords + pNode2->sizeInDwords <= pNode1->offsetInDwords); // Not overlapped
                mergedNode = *pNode1;

                nodeOffset = pNode2->offsetInDwords + pNode2->sizeInDwords;
                ++nodeIdx2;
            }
            else
            {
                LLPC_ASSERT((pNode1->type == pNode2->type) && (pNode1->sizeInDwords == pNode2->sizeInDwords));

                auto nodeType       = pNode1->type;
                auto offsetInDwords = pNode1->offsetInDwords;
                auto sizeInDwords   = pNode1->sizeInDwords;
                LLPC_ASSERT(offsetInDwords >= nodeOffset);

                if (nodeType == ResourceMappingNodeType::DescriptorTableVaPtr)
                {
                    // Table pointer, do further merging
                    mergedNode.type           = nodeType;
                    mergedNode.sizeInDwords   = sizeInDwords;
                    mergedNode.offsetInDwords = offsetInDwords;

                    MergeUserDataNode(pNode1->tablePtr.nodeCount,
                                      pNode1->tablePtr.pNext,
                                      pNode2->tablePtr.nodeCount,
                                      pNode2->tablePtr.pNext,
                                      &mergedNode.tablePtr.nodeCount,
                                      &mergedNode.tablePtr.pNext);
                }
                else
                {
                    // Not table pointer, all fields should be identical
                    LLPC_ASSERT(memcmp(pNode1, pNode2, sizeof(ResourceMappingNode)) == 0);
                    mergedNode = *pNode1;
                }

                nodeOffset = offsetInDwords + sizeInDwords;
                ++nodeIdx1;
                ++nodeIdx2;
            }

            mergedNodes.push_back(mergedNode);
        }

        // Handle the remaining part of the list if it is not finished
        while (nodeIdx1 < nodeCount1)
        {
            const ResourceMappingNode* pNode1 = nodes1[nodeIdx1];
            mergedNodes.push_back(*pNode1);
            ++nodeIdx1;
        }

        // Handle the remaining part of the list if it is not finished
        while (nodeIdx2 < nodeCount2)
        {
            const ResourceMappingNode* pNode2 = nodes2[nodeIdx2];
            mergedNodes.push_back(*pNode2);
            ++nodeIdx2;
        }

        mergedNodeCount = mergedNodes.size();

        pMergedNodes = new ResourceMappingNode[mergedNodeCount];
        LLPC_ASSERT(pMergedNodes != nullptr);
        memcpy(pMergedNodes, mergedNodes.data(), sizeof(ResourceMappingNode) * mergedNodeCount);

        // Record those allocated user data nodes (will be freed during cleanup)
        m_allocUserDataNodes.push_back(pMergedNodes);
    }
    else if (nodeCount1 > 0)
    {
        mergedNodeCount = nodeCount1;
        pMergedNodes = const_cast<ResourceMappingNode*>(pNodes1);
    }
    else if (nodeCount2 > 0)
    {
        mergedNodeCount = nodeCount2;
        pMergedNodes = const_cast<ResourceMappingNode*>(pNodes2);
    }

    *pMergedNodeCount = mergedNodeCount;
    *ppMergedNodes = pMergedNodes;
}

} // Llpc
