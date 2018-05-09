/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpcGfx9ConfigBuilder.cpp
 * @brief LLPC header file: contains implementation of class Llpc::Gfx9::ConfigBuilder.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-gfx9-config-builder"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcCodeGenManager.h"
#include "llpcCopyShader.h"
#include "llpcElf.h"
#include "llpcGfx9ConfigBuilder.h"

namespace Llpc
{

namespace Gfx9
{

#include "gfx9_plus_merged_enum.h"
#include "gfx9_plus_merged_mask.h"
#include "gfx9_plus_merged_offset.h"
#include "gfx9_plus_merged_shift.h"

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-FS).
Result ConfigBuilder::BuildPipelineVsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    uint8_t**           ppConfig,         // [out] Register configuration for VS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    uint8_t* pAllocBuf = new uint8_t[sizeof(PipelineVsFsRegConfig)];
    PipelineVsFsRegConfig* pConfig = reinterpret_cast<PipelineVsFsRegConfig*>(pAllocBuf);
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderVs,
                            0,
                            0,
                            0,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, MAX_PRIMGRP_IN_WAVE, 2);

    if (stageMask & ShaderStageToMask(ShaderStageVertex))
    {
        result = BuildVsRegConfig<PipelineVsFsRegConfig>(pContext, ShaderStageVertex, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_REAL);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        const auto pIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        result = BuildPsRegConfig<PipelineVsFsRegConfig>(pContext, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));
    }

    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    // When non-patch primitives are used without tessellation enabled, PRIMGROUP_SIZE must be at least 4, and must be
    // even if there are more than 2 shader engines on the GPU.
    uint32_t primGroupSize = 128;
    uint32_t numShaderEngines = pContext->GetGpuProperty()->numShaderEngines;
    if (numShaderEngines > 2)
    {
        primGroupSize = Pow2Align(primGroupSize, 2);
    }

    iaMultiVgtParam.bits.PRIMGROUP_SIZE = primGroupSize - 1;

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pAllocBuf;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-TS-FS).
Result ConfigBuilder::BuildPipelineVsTsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    uint8_t**           ppConfig,         // [out] Register configuration for VS-TS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;
    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    uint8_t* pAllocBuf = new uint8_t[sizeof(PipelineVsTsFsRegConfig)];
    PipelineVsTsFsRegConfig* pConfig = reinterpret_cast<PipelineVsTsFsRegConfig*>(pAllocBuf);
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderVs,
                            0,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, MAX_PRIMGRP_IN_WAVE, 2);

    if (stageMask & (ShaderStageToMask(ShaderStageVertex) | ShaderStageToMask(ShaderStageTessControl)))
    {
        const bool hasVs  = ((stageMask & ShaderStageToMask(ShaderStageVertex)) != 0);
        const bool hasTcs = ((stageMask & ShaderStageToMask(ShaderStageTessControl)) != 0);

        result = BuildLsHsRegConfig<PipelineVsTsFsRegConfig>(pContext,
                                                             hasVs ? ShaderStageVertex : ShaderStageInvalid,
                                                             hasTcs ? ShaderStageTessControl : ShaderStageInvalid,
                                                             pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        hash64 = pContext->GetShaderHashCode(ShaderStageTessControl);
        SET_REG(pConfig, API_HS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_HS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, HS_EN, HS_STAGE_ON);
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, LS_EN, LS_STAGE_ON);

        const auto pVsIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pVsIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pVsIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageTessEval)))
    {
        result = BuildVsRegConfig<PipelineVsTsFsRegConfig>(pContext, ShaderStageTessEval, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_DS);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessEval);
        SET_REG(pConfig, API_DS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_DS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        result = BuildPsRegConfig<PipelineVsTsFsRegConfig>(pContext, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    const auto& tcsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
    const auto& tesBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;

    if (tcsBuiltInUsage.primitiveId || tesBuiltInUsage.primitiveId)
    {
        iaMultiVgtParam.bits.PARTIAL_ES_WAVE_ON = true;
        iaMultiVgtParam.bits.SWITCH_ON_EOI = true;
    }

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pAllocBuf;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-GS-FS).
Result ConfigBuilder::BuildPipelineVsGsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    uint8_t**           ppConfig,         // [out] Register configuration for VS-GS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;
    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    uint8_t* pAllocBuf = new uint8_t[sizeof(PipelineVsGsFsRegConfig)];
    PipelineVsGsFsRegConfig* pConfig = reinterpret_cast<PipelineVsGsFsRegConfig*>(pAllocBuf);
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderGs,
                            0,
                            0,
                            Util::Abi::HwShaderGs | Util::Abi::HwShaderVs,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, MAX_PRIMGRP_IN_WAVE, 2);

    if (stageMask & (ShaderStageToMask(ShaderStageVertex) | ShaderStageToMask(ShaderStageGeometry)))
    {
        const bool hasVs = ((stageMask & ShaderStageToMask(ShaderStageVertex)) != 0);
        const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

        result = BuildEsGsRegConfig<PipelineVsGsFsRegConfig>(pContext,
                                                             hasVs ? ShaderStageVertex : ShaderStageInvalid,
                                                             hasGs ? ShaderStageGeometry : ShaderStageInvalid,
                                                             pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        hash64 = pContext->GetShaderHashCode(ShaderStageGeometry);
        SET_REG(pConfig, API_GS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_GS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, ES_EN, ES_STAGE_REAL);
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, GS_EN, GS_STAGE_ON);

        const auto pVsIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pVsIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pVsIntfData->vbTable.resNodeIdx);
        }

        if (gfxIp.major == 9)
        {
            regVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09 vgtGsMaxPrimsPerSubgroup = {};
            vgtGsMaxPrimsPerSubgroup.bits.MAX_PRIMS_PER_SUBGROUP =
                GET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_ONCHIP_CNTL, GS_INST_PRIMS_IN_SUBGRP) *
                GET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MAX_VERT_OUT, MAX_VERT_OUT);
            SET_DYN_REG(pConfig, mmVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09, vgtGsMaxPrimsPerSubgroup.u32All);
        }
        else
        {
            LLPC_NOT_IMPLEMENTED();
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        result = BuildPsRegConfig<PipelineVsGsFsRegConfig>(pContext, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageCopyShader)))
    {
        result = BuildVsRegConfig<PipelineVsGsFsRegConfig>(pContext, ShaderStageCopyShader, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_COPY_SHADER);
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    const uint32_t primGroupSize = 128;
    iaMultiVgtParam.bits.PRIMGROUP_SIZE = primGroupSize - 1;

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pAllocBuf;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-TS-GS-FS).
Result ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    uint8_t**           ppConfig,         // [out] Register configuration for VS-TS-GS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;
    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    uint8_t* pAllocBuf = new uint8_t[sizeof(PipelineVsTsGsFsRegConfig)];
    PipelineVsTsGsFsRegConfig* pConfig = reinterpret_cast<PipelineVsTsGsFsRegConfig*>(pAllocBuf);
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderGs,
                            Util::Abi::HwShaderGs | Util::Abi::HwShaderVs,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, MAX_PRIMGRP_IN_WAVE, 2);

    if (stageMask & (ShaderStageToMask(ShaderStageVertex) | ShaderStageToMask(ShaderStageTessControl)))
    {
        const bool hasVs  = ((stageMask & ShaderStageToMask(ShaderStageVertex)) != 0);
        const bool hasTcs = ((stageMask & ShaderStageToMask(ShaderStageTessControl)) != 0);

        result = BuildLsHsRegConfig<PipelineVsTsGsFsRegConfig>(pContext,
                                                               hasVs ? ShaderStageVertex : ShaderStageInvalid,
                                                               hasTcs ? ShaderStageTessControl : ShaderStageInvalid,
                                                               pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        hash64 = pContext->GetShaderHashCode(ShaderStageTessControl);
        SET_REG(pConfig, API_HS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_HS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, HS_EN, HS_STAGE_ON);
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, LS_EN, LS_STAGE_ON);

        const auto pVsIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pVsIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pVsIntfData->vbTable.resNodeIdx);
        }
    }

    if (stageMask & (ShaderStageToMask(ShaderStageTessEval) | ShaderStageToMask(ShaderStageGeometry)))
    {
        const bool hasTes = ((stageMask & ShaderStageToMask(ShaderStageTessEval)) != 0);
        const bool hasGs  = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

        result = BuildEsGsRegConfig<PipelineVsTsGsFsRegConfig>(pContext,
                                                               hasTes ? ShaderStageTessEval : ShaderStageInvalid,
                                                               hasGs ? ShaderStageGeometry : ShaderStageInvalid,
                                                               pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessEval);
        SET_REG(pConfig, API_DS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_DS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        hash64 = pContext->GetShaderHashCode(ShaderStageGeometry);
        SET_REG(pConfig, API_GS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_GS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, ES_EN, ES_STAGE_DS);
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, GS_EN, GS_STAGE_ON);

        if (gfxIp.major == 9)
        {
            regVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09 vgtGsMaxPrimsPerSubgroup = {};
            vgtGsMaxPrimsPerSubgroup.bits.MAX_PRIMS_PER_SUBGROUP =
                GET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_ONCHIP_CNTL, GS_INST_PRIMS_IN_SUBGRP) *
                GET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MAX_VERT_OUT, MAX_VERT_OUT);
            SET_DYN_REG(pConfig, mmVGT_GS_MAX_PRIMS_PER_SUBGROUP__GFX09, vgtGsMaxPrimsPerSubgroup.u32All);
        }
        else
        {
            LLPC_NOT_IMPLEMENTED();
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        result = BuildPsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_DWORD0, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageCopyShader)))
    {
        result = BuildVsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, ShaderStageCopyShader, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_COPY_SHADER);
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    const auto& tcsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
    const auto& tesBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;
    const auto& gsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageGeometry)->builtInUsage.gs;

    if (tcsBuiltInUsage.primitiveId || tesBuiltInUsage.primitiveId || gsBuiltInUsage.primitiveId)
    {
        iaMultiVgtParam.bits.SWITCH_ON_EOI = true;
    }

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    // Set up VGT_TF_PARAM
    SetupVgtTfParam(pContext, &pConfig->m_lsHsRegs);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pAllocBuf;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for compute pipeline.
Result ConfigBuilder::BuildPipelineCsRegConfig(
    Context*            pContext,        // [in] LLPC context
    uint8_t**           ppConfig,        // [out] Register configuration for compute pipeline
    size_t*             pConfigSize)     // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    LLPC_ASSERT(stageMask == ShaderStageToMask(ShaderStageCompute));

    uint64_t hash64 = 0;

    uint8_t* pAllocBuf = new uint8_t[sizeof(PipelineCsRegConfig)];
    PipelineCsRegConfig* pConfig = reinterpret_cast<PipelineCsRegConfig*>(pAllocBuf);
    pConfig->Init();

    BuildApiHwShaderMapping(0,
                            0,
                            0,
                            0,
                            0,
                            Util::Abi::HwShaderCs,
                            pConfig);
    result = BuildCsRegConfig(pContext, ShaderStageCompute, pConfig);

    hash64 = pContext->GetShaderHashCode(ShaderStageCompute);
    SET_REG(pConfig, API_CS_HASH_DWORD0, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, API_CS_HASH_DWORD1, static_cast<uint32_t>(hash64 >> 32));

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pAllocBuf;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware vertex shader.
template <typename T>
Result ConfigBuilder::BuildVsRegConfig(
    Context*            pContext,       // [in] LLPC context
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for vertex-shader-specific pipeline
{
    Result result = Result::Success;

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    LLPC_ASSERT((shaderStage == ShaderStageVertex)   ||
                (shaderStage == ShaderStageTessEval) ||
                (shaderStage == ShaderStageCopyShader));

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage;

    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, DX10_CLAMP, true);  // Follow PAL setting

    if (gfxIp.major == 9)
    {
        if (shaderStage == ShaderStageCopyShader)
        {
            // NOTE: For copy shader, we use fixed number of user data registers.
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, USER_SGPR, Llpc::CopyShaderUserSgprCount);
            SET_REG(&pConfig->m_vsRegs, VS_NUM_AVAIL_SGPRS, pContext->GetGpuProperty()->maxSgprsAvailable);
            SET_REG(&pConfig->m_vsRegs, VS_NUM_AVAIL_VGPRS, pContext->GetGpuProperty()->maxVgprsAvailable);
        }
        else
        {
            const auto pShaderInfo = pContext->GetPipelineShaderInfo(shaderStage);
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, DEBUG_MODE, pShaderInfo->options.debugMode);

            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, TRAP_PRESENT, pShaderInfo->options.trapPresent);
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, USER_SGPR, pIntfData->userDataCount);

            SET_REG(&pConfig->m_vsRegs, VS_NUM_AVAIL_SGPRS, pResUsage->numSgprsAvailable);
            SET_REG(&pConfig->m_vsRegs, VS_NUM_AVAIL_VGPRS, pResUsage->numVgprsAvailable);
        }
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(pContext->GetPipelineBuildInfo());

    uint8_t usrClipPlaneMask = pPipelineInfo->rsState.usrClipPlaneMask;
    bool depthClipDisable = (pPipelineInfo->vpState.depthClipEnable == false);
    bool rasterizerDiscardEnable = pPipelineInfo->rsState.rasterizerDiscardEnable;
    bool disableVertexReuse = pPipelineInfo->iaState.disableVertexReuse;

    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_0, (usrClipPlaneMask >> 0) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_1, (usrClipPlaneMask >> 1) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_2, (usrClipPlaneMask >> 2) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_3, (usrClipPlaneMask >> 3) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_4, (usrClipPlaneMask >> 4) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, UCP_ENA_5, (usrClipPlaneMask >> 5) & 0x1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, DX_LINEAR_ATTR_CLIP_ENA,true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, DX_CLIP_SPACE_DEF, true); // DepthRange::ZeroToOne
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, ZCLIP_NEAR_DISABLE,depthClipDisable);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, ZCLIP_FAR_DISABLE, depthClipDisable);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, DX_RASTERIZATION_KILL,rasterizerDiscardEnable);

    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_X_SCALE_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_X_OFFSET_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_Y_SCALE_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_Y_OFFSET_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_Z_SCALE_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VPORT_Z_OFFSET_ENA, true);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VTE_CNTL, VTX_W0_FMT, true);

    SET_REG_FIELD(&pConfig->m_vsRegs, PA_SU_VTX_CNTL, PIX_CENTER, 1);
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_SU_VTX_CNTL, ROUND_MODE, 2); // Round to even
    SET_REG_FIELD(&pConfig->m_vsRegs, PA_SU_VTX_CNTL, QUANT_MODE, 5); // Use 8-bit fractions

    // Stage-specific processing
    bool usePointSize = false;
    bool usePrimitiveId = false;
    bool useLayer = false;
    bool useViewportIndex = false;
    uint32_t clipDistanceCount = 0;
    uint32_t cullDistanceCount = 0;

    if (shaderStage == ShaderStageVertex)
    {
        usePointSize      = builtInUsage.vs.pointSize;
        usePrimitiveId    = builtInUsage.vs.primitiveId;
        useLayer          = builtInUsage.vs.layer;
        useViewportIndex  = builtInUsage.vs.viewportIndex;
        clipDistanceCount = builtInUsage.vs.clipDistance;
        cullDistanceCount = builtInUsage.vs.cullDistance;

        if (builtInUsage.vs.instanceIndex)
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 3); // 3: Enable instance ID
        }
        else if (builtInUsage.vs.primitiveId)
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 2);
        }
    }
    else if (shaderStage == ShaderStageTessEval)
    {
        usePointSize      = builtInUsage.tes.pointSize;
        usePrimitiveId    = builtInUsage.tes.primitiveId;
        useLayer          = builtInUsage.tes.layer;
        useViewportIndex  = builtInUsage.tes.viewportIndex;
        clipDistanceCount = builtInUsage.tes.clipDistance;
        cullDistanceCount = builtInUsage.tes.cullDistance;

        if (builtInUsage.tes.primitiveId)
        {
            // NOTE: when primitive ID is used, set vgtCompCnt to 3 directly because primitive ID is the last VGPR.
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 3); // 3: Enable primitive ID
        }
        else
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 2);
        }

        if (pContext->IsTessOffChip())
        {
            if (gfxIp.major == 9)
            {
                SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, OC_LDS_EN, true);
            }
            else
            {
                LLPC_NOT_IMPLEMENTED();
            }
        }
    }
    else
    {
        LLPC_ASSERT(shaderStage == ShaderStageCopyShader);

        usePointSize      = builtInUsage.gs.pointSize;
        usePrimitiveId    = builtInUsage.gs.primitiveIdIn;
        useLayer          = builtInUsage.gs.layer;
        useViewportIndex  = builtInUsage.gs.viewportIndex;
        clipDistanceCount = builtInUsage.gs.clipDistance;
        cullDistanceCount = builtInUsage.gs.cullDistance;

        // NOTE: For ES-GS merged shader, the actual use of primitive ID should take both ES and GS into consideration.
        const bool hasTs = ((pContext->GetShaderStageMask() & (ShaderStageToMask(ShaderStageTessControl) |
                                                               ShaderStageToMask(ShaderStageTessEval))) != 0);
        if (hasTs)
        {
            const auto& tesBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;
            usePrimitiveId = usePrimitiveId || tesBuiltInUsage.primitiveId;
        }
        else
        {
            const auto& vsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;
            usePrimitiveId = usePrimitiveId || vsBuiltInUsage.primitiveId;
        }
    }

    SET_REG_FIELD(&pConfig->m_vsRegs, VGT_PRIMITIVEID_EN, PRIMITIVEID_EN, usePrimitiveId);
    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_VS_OUT_CONFIG, VS_EXPORT_COUNT, pResUsage->inOutUsage.expCount - 1);

    SET_REG(&pConfig->m_vsRegs, USES_VIEWPORT_ARRAY_INDEX, useViewportIndex);

    // According to the IA_VGT_Spec, it is only legal to enable vertex reuse when we're using viewport array
    // index if each GS, DS, or VS invocation emits the same viewport array index for each vertex and we set
    // VTE_VPORT_PROVOKE_DISABLE.
    if (useViewportIndex)
    {
        // TODO: In the future, we can only disable vertex reuse only if viewport array index is emitted divergently
        // for each vertex.
        disableVertexReuse = true;
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, VTE_VPORT_PROVOKE_DISABLE, true);
    }
    else
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_CLIP_CNTL, VTE_VPORT_PROVOKE_DISABLE, false);
    }

    SET_REG_FIELD(&pConfig->m_vsRegs, VGT_REUSE_OFF, REUSE_OFF, disableVertexReuse);

    useLayer = useLayer || pPipelineInfo->iaState.enableMultiView;

    if (usePointSize || useLayer || useViewportIndex)
    {
        if (gfxIp.major == 9)
        {
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_POINT_SIZE, usePointSize);
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_RENDER_TARGET_INDX, useLayer);
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_VIEWPORT_INDX, useViewportIndex);
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_MISC_VEC_ENA, true);
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_MISC_SIDE_BUS_ENA, true);
        }
        else
        {
            LLPC_NOT_IMPLEMENTED();
        }
    }

    if ((clipDistanceCount > 0) || (cullDistanceCount > 0))
    {
        if (gfxIp.major == 9)
        {
            SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_CCDIST0_VEC_ENA, true);
            if (clipDistanceCount + cullDistanceCount > 4)
            {
                SET_REG_FIELD_GFX9(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_CCDIST1_VEC_ENA, true);
            }
        }
        else
        {
            LLPC_NOT_IMPLEMENTED();
        }

        uint32_t clipDistanceMask = (1 << clipDistanceCount) - 1;
        uint32_t cullDistanceMask = (1 << cullDistanceCount) - 1;

        // Set fields CLIP_DIST_ENA_0 ~ CLIP_DIST_ENA_7 and CULL_DIST_ENA_0 ~ CULL_DIST_ENA_7
        uint32_t paClVsOutCntl = GET_REG(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL);
        paClVsOutCntl |= clipDistanceMask;
        paClVsOutCntl |= (cullDistanceMask << 8);
        SET_REG(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, paClVsOutCntl);
    }

    uint32_t posCount = 1; // gl_Position is always exported
    if (usePointSize || useLayer || useViewportIndex)
    {
        ++posCount;
    }

    if (clipDistanceCount + cullDistanceCount > 0)
    {
        ++posCount;
        if (clipDistanceCount + cullDistanceCount > 4)
        {
            ++posCount;
        }
    }

    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_POS_FORMAT, POS0_EXPORT_FORMAT, SPI_SHADER_4COMP);
    if (posCount > 1)
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_POS_FORMAT, POS1_EXPORT_FORMAT, SPI_SHADER_4COMP);
    }
    if (posCount > 2)
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_POS_FORMAT, POS2_EXPORT_FORMAT, SPI_SHADER_4COMP);
    }
    if (posCount > 3)
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_POS_FORMAT, POS3_EXPORT_FORMAT, SPI_SHADER_4COMP);
    }

    // Set shader user data maping
    if (result == Result::Success)
    {
        result = ConfigBuilder::BuildUserDataConfig<T>(pContext,
                                                       shaderStage,
                                                       ShaderStageInvalid,
                                                       mmSPI_SHADER_USER_DATA_VS_0,
                                                       pConfig);
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware local-hull merged shader.
template <typename T>
Result ConfigBuilder::BuildLsHsRegConfig(
    Context*            pContext,       // [in] LLPC context
    ShaderStage         shaderStage1,   // Current first shader stage (from API side)
    ShaderStage         shaderStage2,   // Current second shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for local-hull-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT((shaderStage1 == ShaderStageVertex) || (shaderStage1 == ShaderStageInvalid));
    LLPC_ASSERT((shaderStage2 == ShaderStageTessControl) || (shaderStage2 == ShaderStageInvalid));

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    const auto pTcsResUsage = pContext->GetShaderResourceUsage(ShaderStageTessControl);
    const auto& vsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;
    const auto& tcsBuiltInUsage = pTcsResUsage->builtInUsage.tcs;

    SET_REG_FIELD(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC1_HS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC1_HS, DX10_CLAMP, true); // Follow PAL setting

    uint32_t lsVgtCompCnt = 1;
    if (vsBuiltInUsage.instanceIndex)
    {
        lsVgtCompCnt += 2; // Enable instance ID
    }
    SET_REG_FIELD(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC1_HS, LS_VGPR_COMP_CNT, lsVgtCompCnt);

    const auto& pVsIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
    const auto& pTcsIntfData = pContext->GetShaderInterfaceData(ShaderStageTessControl);
    uint32_t userDataCount = std::max(pVsIntfData->userDataCount, pTcsIntfData->userDataCount);

    const auto pTcsShaderInfo = pContext->GetPipelineShaderInfo(ShaderStageTessControl);
    SET_REG_FIELD(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC1_HS, DEBUG_MODE, pTcsShaderInfo->options.debugMode);
    if (gfxIp.major == 9)
    {

        SET_REG_FIELD_GFX9(&pConfig->m_lsHsRegs,
                           SPI_SHADER_PGM_RSRC2_HS,
                           TRAP_PRESENT,
                           pTcsShaderInfo->options.trapPresent);
        SET_REG_FIELD_GFX9(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC2_HS, USER_SGPR, userDataCount);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    // NOTE: On GFX7+, granularity for the LDS_SIZE field is 128. The range is 0~128 which allocates 0 to 16K
    // DWORDs.
    const auto& calcFactor = pTcsResUsage->inOutUsage.tcs.calcFactor;
    uint32_t ldsSizeInDwords = calcFactor.onChip.patchConstStart +
                               calcFactor.patchConstSize * calcFactor.patchCountPerThreadGroup;
    if (pContext->IsTessOffChip())
    {
        ldsSizeInDwords = calcFactor.inPatchSize * calcFactor.patchCountPerThreadGroup;
    }

    const uint32_t ldsSizeDwordGranularity = 128u;
    const uint32_t ldsSizeDwordGranularityShift = 7u;
    uint32_t ldsSize = Pow2Align(ldsSizeInDwords, ldsSizeDwordGranularity) >> ldsSizeDwordGranularityShift;

    if (gfxIp.major == 9)
    {
        SET_REG_FIELD_GFX9(&pConfig->m_lsHsRegs, SPI_SHADER_PGM_RSRC2_HS, LDS_SIZE, ldsSize);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    // Minimum and maximum tessellation factors supported by the hardware.
    constexpr float MinTessFactor = 1.0f;
    constexpr float MaxTessFactor = 64.0f;
    SET_REG(&pConfig->m_lsHsRegs, VGT_HOS_MIN_TESS_LEVEL, FloatToBits(MinTessFactor));
    SET_REG(&pConfig->m_lsHsRegs, VGT_HOS_MAX_TESS_LEVEL, FloatToBits(MaxTessFactor));

    // Set VGT_LS_HS_CONFIG
    SET_REG_FIELD(&pConfig->m_lsHsRegs, VGT_LS_HS_CONFIG, NUM_PATCHES, calcFactor.patchCountPerThreadGroup);
    auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(pContext->GetPipelineBuildInfo());
    SET_REG_FIELD(&pConfig->m_lsHsRegs, VGT_LS_HS_CONFIG, HS_NUM_INPUT_CP, pPipelineInfo->iaState.patchControlPoints);

    auto hsNumOutputCp = tcsBuiltInUsage.outputVertices;
    SET_REG_FIELD(&pConfig->m_lsHsRegs, VGT_LS_HS_CONFIG, HS_NUM_OUTPUT_CP, hsNumOutputCp);

    SET_REG(&pConfig->m_lsHsRegs, HS_NUM_AVAIL_SGPRS, pTcsResUsage->numSgprsAvailable);
    SET_REG(&pConfig->m_lsHsRegs, HS_NUM_AVAIL_VGPRS, pTcsResUsage->numVgprsAvailable);

    // Set up VGT_TF_PARAM
    SetupVgtTfParam(pContext, &pConfig->m_lsHsRegs);

    if (gfxIp.major == 9)
    {
        result = ConfigBuilder::BuildUserDataConfig<T>(
                     pContext,
                     (shaderStage1 != ShaderStageInvalid) ? shaderStage1 : shaderStage2,
                     (shaderStage1 != ShaderStageInvalid) ? shaderStage2 : ShaderStageInvalid,
                     mmSPI_SHADER_USER_DATA_LS_0__GFX09,
                     pConfig);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware export-geometry merged shader.
template <typename T>
Result ConfigBuilder::BuildEsGsRegConfig(
    Context*            pContext,       // [in] LLPC context
    ShaderStage         shaderStage1,   // Current first shader stage (from API side)
    ShaderStage         shaderStage2,   // Current second shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for export-geometry-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT((shaderStage1 == ShaderStageVertex) || (shaderStage1 == ShaderStageTessEval) ||
                (shaderStage1 == ShaderStageInvalid));
    LLPC_ASSERT((shaderStage2 == ShaderStageGeometry) || (shaderStage2 == ShaderStageInvalid));

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    const uint32_t stageMask = pContext->GetShaderStageMask();
    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);

    const auto pVsResUsage = pContext->GetShaderResourceUsage(ShaderStageVertex);
    const auto& vsBuiltInUsage = pVsResUsage->builtInUsage.vs;
    const auto& vsInOutUsage   = pVsResUsage->inOutUsage;

    const auto pTesResUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval);
    const auto& tesBuiltInUsage = pTesResUsage->builtInUsage.tes;
    const auto& tesInOutUsage   = pTesResUsage->inOutUsage;

    const auto pGsResUsage = pContext->GetShaderResourceUsage(ShaderStageGeometry);
    const auto& gsBuiltInUsage = pGsResUsage->builtInUsage.gs;
    const auto& gsInOutUsage   = pGsResUsage->inOutUsage;
    const auto& calcFactor     = gsInOutUsage.gs.calcFactor;

    uint32_t gsVgprCompCnt = 0;
    if ((calcFactor.inputVertices > 4) || gsBuiltInUsage.invocationId)
    {
        gsVgprCompCnt = 3;
    }
    else if (gsBuiltInUsage.primitiveIdIn)
    {
        gsVgprCompCnt = 2;
    }
    else if (calcFactor.inputVertices > 2)
    {
        gsVgprCompCnt = 1;
    }

    SET_REG_FIELD(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC1_GS, GS_VGPR_COMP_CNT, gsVgprCompCnt);

    SET_REG_FIELD(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC1_GS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC1_GS, DX10_CLAMP, true); // Follow PAL setting

    const auto& pVsIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
    const auto& pTesIntfData = pContext->GetShaderInterfaceData(ShaderStageTessEval);
    const auto& pGsIntfData = pContext->GetShaderInterfaceData(ShaderStageGeometry);
    uint32_t userDataCount = std::max(hasTs ? pTesIntfData->userDataCount : pVsIntfData->userDataCount,
                                      pGsIntfData->userDataCount);

    const auto pGsShaderInfo = pContext->GetPipelineShaderInfo(ShaderStageGeometry);
    SET_REG_FIELD(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC1_GS, DEBUG_MODE, pGsShaderInfo->options.debugMode);
    if (gfxIp.major == 9)
    {

        SET_REG_FIELD_GFX9(&pConfig->m_esGsRegs,
                           SPI_SHADER_PGM_RSRC2_GS,
                           TRAP_PRESENT,
                           pGsShaderInfo->options.trapPresent);
        SET_REG_FIELD_GFX9(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC2_GS, USER_SGPR, userDataCount);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    uint32_t esVgprCompCnt = 0;
    if (hasTs)
    {
        // NOTE: when primitive ID is used, set vgtCompCnt to 3 directly because primitive ID is the last VGPR.
        if (tesBuiltInUsage.primitiveId)
        {
            esVgprCompCnt = 3;
        }
        else
        {
            esVgprCompCnt = 2;
        }

        if (pContext->IsTessOffChip())
        {
            if (gfxIp.major == 9)
            {
                SET_REG_FIELD_GFX9(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC2_GS, OC_LDS_EN, true);
            }
            else
            {
                LLPC_NOT_IMPLEMENTED();
            }
        }
    }
    else
    {
        if (vsBuiltInUsage.instanceIndex)
        {
            esVgprCompCnt = 3; // Enable instance ID
        }
    }

    if (gfxIp.major == 9)
    {
        SET_REG_FIELD_GFX9(&pConfig->m_esGsRegs, SPI_SHADER_PGM_RSRC2_GS, ES_VGPR_COMP_CNT, esVgprCompCnt);

        const auto ldsSizeDwordGranularityShift = pContext->GetGpuProperty()->ldsSizeDwordGranularityShift;

        SET_REG_FIELD_GFX9(&pConfig->m_esGsRegs,
                           SPI_SHADER_PGM_RSRC2_GS,
                           LDS_SIZE,
                           calcFactor.gsOnChipLdsSize >> ldsSizeDwordGranularityShift);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    uint32_t maxVertOut = std::max(1u, static_cast<uint32_t>(gsBuiltInUsage.outputVertices));
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MAX_VERT_OUT, MAX_VERT_OUT, maxVertOut);

    // TODO: Currently only support offchip GS
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, MODE, GS_SCENARIO_G);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, ONCHIP, VGT_GS_MODE_ONCHIP_OFF);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, ES_WRITE_OPTIMIZE, false);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, GS_WRITE_OPTIMIZE, true);
    if (gsBuiltInUsage.outputVertices <= 128)
    {
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_128);
    }
    else if (gsBuiltInUsage.outputVertices <= 256)
    {
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_256);
    }
    else if (gsBuiltInUsage.outputVertices <= 512)
    {
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_512);
    }
    else
    {
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_1024);
    }

    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_ONCHIP_CNTL, ES_VERTS_PER_SUBGRP, calcFactor.esVertsPerSubgroup);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_ONCHIP_CNTL, GS_PRIMS_PER_SUBGRP, calcFactor.gsPrimsPerSubgroup);
    SET_REG_FIELD(&pConfig->m_esGsRegs,
                  VGT_GS_ONCHIP_CNTL,
                  GS_INST_PRIMS_IN_SUBGRP,
                  (gsBuiltInUsage.invocations > 1) ? (calcFactor.gsPrimsPerSubgroup * gsBuiltInUsage.invocations) : 0);

    uint32_t gsVertItemSize = 4 * std::max(1u, gsInOutUsage.outputMapLocCount);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_VERT_ITEMSIZE, ITEMSIZE, gsVertItemSize);

    if ((gsBuiltInUsage.invocations > 1) || gsBuiltInUsage.invocationId)
    {
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_INSTANCE_CNT, ENABLE, true);
        SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_INSTANCE_CNT, CNT, gsBuiltInUsage.invocations);
    }
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_PER_VS, GS_PER_VS, GsThreadsPerVsThread);

    VGT_GS_OUTPRIM_TYPE gsOutputPrimitiveType = TRISTRIP;
    if (gsInOutUsage.outputMapLocCount == 0)
    {
        gsOutputPrimitiveType = POINTLIST;
    }
    else if (gsBuiltInUsage.outputPrimitive == OutputPoints)
    {
        gsOutputPrimitiveType = POINTLIST;
    }
    else if (gsBuiltInUsage.outputPrimitive == LINESTRIP)
    {
        gsOutputPrimitiveType = LINESTRIP;
    }

    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE,   gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_1, gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_2, gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_3, gsOutputPrimitiveType);

    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GSVS_RING_ITEMSIZE, ITEMSIZE, calcFactor.gsVsRingItemSize);

    // TODO: Multiple output streams are not supported.
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GSVS_RING_OFFSET_1, OFFSET, calcFactor.gsVsRingItemSize);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GSVS_RING_OFFSET_2, OFFSET, calcFactor.gsVsRingItemSize);
    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_GSVS_RING_OFFSET_3, OFFSET, calcFactor.gsVsRingItemSize);

    SET_REG_FIELD(&pConfig->m_esGsRegs, VGT_ESGS_RING_ITEMSIZE, ITEMSIZE, calcFactor.esGsRingItemSize);

    SET_REG(&pConfig->m_esGsRegs, GS_NUM_AVAIL_SGPRS, pGsResUsage->numSgprsAvailable);
    SET_REG(&pConfig->m_esGsRegs, GS_NUM_AVAIL_VGPRS, pGsResUsage->numVgprsAvailable);

    result = ConfigBuilder::BuildUserDataConfig<T>(
                 pContext,
                 (shaderStage1 != ShaderStageInvalid) ? shaderStage1 : shaderStage2,
                 (shaderStage1 != ShaderStageInvalid) ? shaderStage2 : ShaderStageInvalid,
                 mmSPI_SHADER_USER_DATA_ES_0,
                 pConfig);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware pixel shader.
template <typename T>
Result ConfigBuilder::BuildPsRegConfig(
    Context*            pContext,       // [in] LLPC context
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for pixel-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageFragment);

    GfxIpVersion gfxIp = pContext->GetGfxIpVersion();

    const GraphicsPipelineBuildInfo* pPipelineInfo =
        static_cast<const GraphicsPipelineBuildInfo*>(pContext->GetPipelineBuildInfo());

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);
    const auto pShaderInfo = pContext->GetPipelineShaderInfo(shaderStage);
    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage.fs;

    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, DX10_CLAMP, true);  // Follow PAL setting
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, DEBUG_MODE, pShaderInfo->options.debugMode);

    if (gfxIp.major == 9)
    {
        SET_REG_FIELD_GFX9(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC2_PS, TRAP_PRESENT, pShaderInfo->options.trapPresent);
        SET_REG_FIELD_GFX9(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC2_PS, USER_SGPR, pIntfData->userDataCount);
    }
    else
    {
        LLPC_NOT_IMPLEMENTED();
    }

    SET_REG_FIELD(&pConfig->m_psRegs, SPI_BARYC_CNTL, FRONT_FACE_ALL_BITS, true);
    if (builtInUsage.pixelCenterInteger)
    {
        // TRUE - Force floating point position to upper left corner of pixel (X.0, Y.0)
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_BARYC_CNTL, POS_FLOAT_ULC, true);
    }
    else if (builtInUsage.runAtSampleRate)
    {
        // 2 - Calculate per-pixel floating point position at iterated sample number
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_BARYC_CNTL, POS_FLOAT_LOCATION, 2);
    }
    else
    {
        // 0 - Calculate per-pixel floating point position at pixel center
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_BARYC_CNTL, POS_FLOAT_LOCATION, 0);
    }

    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, WALK_ALIGN8_PRIM_FITS_ST, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, WALK_FENCE_ENABLE, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, TILE_WALK_ORDER_ENABLE, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, PS_ITER_SAMPLE, builtInUsage.runAtSampleRate);

    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, SUPERTILE_WALK_ORDER_ENABLE, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, MULTI_SHADER_ENGINE_PRIM_DISCARD_ENABLE, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, FORCE_EOV_CNTDWN_ENABLE, true);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_MODE_CNTL_1, FORCE_EOV_REZ_ENABLE, true);

    ZOrder zOrder = LATE_Z;
    bool execOnHeirFail = false;
    if (builtInUsage.earlyFragmentTests)
    {
        zOrder = EARLY_Z_THEN_LATE_Z;
    }
    else if (pResUsage->imageWrite)
    {
        zOrder = LATE_Z;
        execOnHeirFail = true;
    }
    else
    {
        zOrder = EARLY_Z_THEN_LATE_Z;
    }

    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, Z_ORDER, zOrder);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, KILL_ENABLE, builtInUsage.discard);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, Z_EXPORT_ENABLE, builtInUsage.fragDepth);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, STENCIL_TEST_VAL_EXPORT_ENABLE, builtInUsage.fragStencilRef);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, MASK_EXPORT_ENABLE, builtInUsage.sampleMask);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, ALPHA_TO_MASK_DISABLE,
                  (builtInUsage.sampleMask || (pPipelineInfo->cbState.alphaToCoverageEnable == false)));
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, DEPTH_BEFORE_SHADER, builtInUsage.earlyFragmentTests);
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, EXEC_ON_NOOP,
                  (builtInUsage.earlyFragmentTests && pResUsage->imageWrite));
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, EXEC_ON_HIER_FAIL, execOnHeirFail);

    uint32_t depthExpFmt = EXP_FORMAT_ZERO;
    if (builtInUsage.sampleMask)
    {
        depthExpFmt = EXP_FORMAT_32_ABGR;
    }
    else if (builtInUsage.fragStencilRef)
    {
        depthExpFmt = EXP_FORMAT_32_GR;
    }
    else if (builtInUsage.fragDepth)
    {
        depthExpFmt = EXP_FORMAT_32_R;
    }
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_Z_FORMAT, Z_EXPORT_FORMAT, depthExpFmt);

    uint32_t spiShaderColFormat = 0;
    uint32_t cbShaderMask = pResUsage->inOutUsage.fs.cbShaderMask;
    const auto& expFmts = pResUsage->inOutUsage.fs.expFmts;
    for (uint32_t i = 0; i < MaxColorTargets; ++i)
    {
        // Set fields COL0_EXPORT_FORMAT ~ COL7_EXPORT_FORMAT
        spiShaderColFormat |= (expFmts[i] << (4 * i));
    }

    if ((spiShaderColFormat == 0) && (depthExpFmt == EXP_FORMAT_ZERO))
    {
        // NOTE: Hardware requires that fragment shader always exports "something" (color or depth) to the SX.
        // If both SPI_SHADER_Z_FORMAT and SPI_SHADER_COL_FORMAT are zero, we need to override
        // SPI_SHADER_COL_FORMAT to export one channel to MRT0. This dummy export format will be masked
        // off by CB_SHADER_MASK.
        spiShaderColFormat = SPI_SHADER_32_R;
        cbShaderMask = 1;
    }

    SET_REG(&pConfig->m_psRegs, SPI_SHADER_COL_FORMAT, spiShaderColFormat);

    SET_REG(&pConfig->m_psRegs, CB_SHADER_MASK, cbShaderMask);
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_PS_IN_CONTROL, NUM_INTERP, pResUsage->inOutUsage.fs.interpInfo.size());

    const auto& interpInfo = pResUsage->inOutUsage.fs.interpInfo;
    uint32_t pointCoordLoc = InvalidValue;
    if (pResUsage->inOutUsage.builtInInputLocMap.find(spv::BuiltInPointCoord) !=
        pResUsage->inOutUsage.builtInInputLocMap.end())
    {
        // Get generic input corresponding to gl_PointCoord (to set the field PT_SPRITE_TEX)
        pointCoordLoc = pResUsage->inOutUsage.builtInInputLocMap[spv::BuiltInPointCoord];
    }

    for (uint32_t i = 0; i < interpInfo.size(); ++i)
    {
        LLPC_ASSERT(((interpInfo[i].loc     == InvalidFsInterpInfo.loc) &&
                     (interpInfo[i].flat    == InvalidFsInterpInfo.flat) &&
                     (interpInfo[i].custom  == InvalidFsInterpInfo.custom)) == false);

        regSPI_PS_INPUT_CNTL_0 spiPsInputCntl = {};
        spiPsInputCntl.bits.FLAT_SHADE = interpInfo[i].flat;
        spiPsInputCntl.bits.OFFSET = interpInfo[i].loc;

        if (interpInfo[i].custom)
        {
            // NOTE: Force parameter cache data to be read in passthrough mode.
            static const uint32_t PassThroughMode = (1 << 5);
            spiPsInputCntl.bits.FLAT_SHADE = true;
            spiPsInputCntl.bitfields.OFFSET |= PassThroughMode;
        }

        if (pointCoordLoc == i)
        {
            spiPsInputCntl.bits.PT_SPRITE_TEX = true;

            // NOTE: Set the offset value to force hardware to select input defaults (no VS match).
            static const uint32_t UseDefaultVal = (1 << 5);
            spiPsInputCntl.bitfields.OFFSET = UseDefaultVal;
        }

        SET_DYN_REG(pConfig, mmSPI_PS_INPUT_CNTL_0 + i, spiPsInputCntl.u32All);
    }

    if (pointCoordLoc != InvalidValue)
    {
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_INTERP_CONTROL_0, PNT_SPRITE_ENA, true);
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_INTERP_CONTROL_0, PNT_SPRITE_OVRD_X, SPI_PNT_SPRITE_SEL_S);
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_INTERP_CONTROL_0, PNT_SPRITE_OVRD_Y, SPI_PNT_SPRITE_SEL_T);
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_INTERP_CONTROL_0, PNT_SPRITE_OVRD_Z, SPI_PNT_SPRITE_SEL_0);
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_INTERP_CONTROL_0, PNT_SPRITE_OVRD_W, SPI_PNT_SPRITE_SEL_1);
    }

    SET_REG(&pConfig->m_psRegs, PS_USES_UAVS, static_cast<uint32_t>(pResUsage->imageWrite));

    if (pPipelineInfo->rsState.innerCoverage)
    {
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_AA_CONFIG, COVERAGE_TO_SHADER_SELECT, INPUT_INNER_COVERAGE);
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, COVERAGE_AA_MASK_ENABLE, false);
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, UNDER_RAST_ENABLE, true);
    }
    else
    {
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_AA_CONFIG, COVERAGE_TO_SHADER_SELECT, INPUT_COVERAGE);
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, COVERAGE_AA_MASK_ENABLE, true);
        SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL, UNDER_RAST_ENABLE, false);
    }

    const uint32_t spiShaderPgmRsrc2Ps = GET_REG(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC2_PS);
    const uint32_t loadCollisionWaveId =
        (spiShaderPgmRsrc2Ps & SPI_SHADER_PGM_RSRC2_PS__LOAD_COLLISION_WAVEID_MASK) >>
         SPI_SHADER_PGM_RSRC2_PS__LOAD_COLLISION_WAVEID__SHIFT;
    const uint32_t  loadIntrawaveCollision =
        (spiShaderPgmRsrc2Ps & SPI_SHADER_PGM_RSRC2_PS__LOAD_INTRAWAVE_COLLISION_MASK) >>
         SPI_SHADER_PGM_RSRC2_PS__LOAD_INTRAWAVE_COLLISION__SHIFT;

    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_SHADER_CONTROL, LOAD_COLLISION_WAVEID, loadCollisionWaveId);
    SET_REG_FIELD(&pConfig->m_psRegs, PA_SC_SHADER_CONTROL, LOAD_INTRAWAVE_COLLISION, loadIntrawaveCollision);

    SET_REG(&pConfig->m_psRegs, PS_NUM_AVAIL_SGPRS, pResUsage->numSgprsAvailable);
    SET_REG(&pConfig->m_psRegs, PS_NUM_AVAIL_VGPRS, pResUsage->numVgprsAvailable);

    // Set shader user data mapping
    if (result == Result::Success)
    {
        result = ConfigBuilder::BuildUserDataConfig<T>(pContext,
                                                       shaderStage,
                                                       ShaderStageInvalid,
                                                       mmSPI_SHADER_USER_DATA_PS_0,
                                                       pConfig);
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for compute shader.
Result ConfigBuilder::BuildCsRegConfig(
    Context*             pContext,      // [in] LLPC context
    ShaderStage          shaderStage,   // Current shader stage (from API side)
    PipelineCsRegConfig* pConfig)       // [out] Register configuration for compute pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageCompute);

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);
    const auto pShaderInfo = pContext->GetPipelineShaderInfo(shaderStage);
    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage.cs;

    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, DX10_CLAMP, true);  // Follow PAL setting
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, DEBUG_MODE, pShaderInfo->options.debugMode);

    // Set registers based on shader interface data
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TRAP_PRESENT, pShaderInfo->options.trapPresent);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, USER_SGPR, pIntfData->userDataCount);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_X_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_Y_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_Z_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TG_SIZE_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TIDIG_COMP_CNT, (builtInUsage.localInvocationId ? 2 : 0));

    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_X, NUM_THREAD_FULL, builtInUsage.workgroupSizeX);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_Y, NUM_THREAD_FULL, builtInUsage.workgroupSizeY);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_Z, NUM_THREAD_FULL, builtInUsage.workgroupSizeZ);

    SET_REG(&pConfig->m_csRegs, CS_NUM_AVAIL_SGPRS, pResUsage->numSgprsAvailable);
    SET_REG(&pConfig->m_csRegs, CS_NUM_AVAIL_VGPRS, pResUsage->numVgprsAvailable);

    // Set shader user data mapping
    if (result == Result::Success)
    {
        result = ConfigBuilder::BuildUserDataConfig<PipelineCsRegConfig>(pContext,
                                                                         shaderStage,
                                                                         ShaderStageInvalid,
                                                                         mmCOMPUTE_USER_DATA_0,
                                                                         pConfig);
    }

    return result;
}

// =====================================================================================================================
// Builds user data configuration for the specified shader stage.
template <typename T>
Result ConfigBuilder::BuildUserDataConfig(
    Context*    pContext,       // [in] LLPC context
    ShaderStage shaderStage1,   // Current first shader stage (from API side)
    ShaderStage shaderStage2,   // Current second shader stage (from API side)
    uint32_t    startUserData,  // Starting user data
    T*          pConfig)        // [out] Register configuration for the associated pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage1 != ShaderStageInvalid); // The first shader stage must be a valid one

    // NOTE: For merged shader, the second shader stage should be tessellation control shader (LS-HS) or geometry
    // shader (ES-GS).
    LLPC_ASSERT((shaderStage2 == ShaderStageTessControl) || (shaderStage2 == ShaderStageGeometry) ||
                (shaderStage2 == ShaderStageInvalid));

    uint32_t stageMask = pContext->GetShaderStageMask();
    const bool hasTs = ((stageMask & (ShaderStageToMask(ShaderStageTessControl) |
                                      ShaderStageToMask(ShaderStageTessEval))) != 0);
    const bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    bool enableMultiView = false;
    if (pContext->IsGraphics())
    {
        enableMultiView = static_cast<const GraphicsPipelineBuildInfo*>(
            pContext->GetPipelineBuildInfo())->iaState.enableMultiView;
    }

    const auto pIntfData1 = pContext->GetShaderInterfaceData(shaderStage1);
    const auto& entryArgIdxs1 = pIntfData1->entryArgIdxs;

    const auto pResUsage1 = pContext->GetShaderResourceUsage(shaderStage1);
    const auto& builtInUsage1 = pResUsage1->builtInUsage;

    const auto pIntfData2 = (shaderStage2 != ShaderStageInvalid) ?
                                pContext->GetShaderInterfaceData(shaderStage2) : nullptr;
    const auto pResUsage2 = (shaderStage2 != ShaderStageInvalid) ?
                                pContext->GetShaderResourceUsage(shaderStage2) : nullptr;

    // Stage-specific processing
    if (shaderStage1 == ShaderStageVertex)
    {
        // TODO: PAL only check BaseVertex now, we need update code once PAL check them separately.
        if (builtInUsage1.vs.baseVertex || builtInUsage1.vs.baseInstance)
        {
            LLPC_ASSERT(entryArgIdxs1.vs.baseVertex > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.vs.baseVertex,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::BaseVertex));

            LLPC_ASSERT(entryArgIdxs1.vs.baseInstance > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.vs.baseInstance,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::BaseInstance));
        }

        if (builtInUsage1.vs.drawIndex)
        {
            LLPC_ASSERT(entryArgIdxs1.vs.drawIndex > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.vs.drawIndex,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::DrawIndex));
        }

        if (enableMultiView)
        {
            if ((shaderStage2 == ShaderStageInvalid) || (shaderStage2 == ShaderStageTessControl))
            {
                // Act as hardware VS or LS-HS merged shader
                LLPC_ASSERT(entryArgIdxs1.vs.viewIndex > 0);
                SET_DYN_REG(pConfig,
                            startUserData + pIntfData1->userDataUsage.vs.viewIndex,
                            static_cast<uint32_t>(Util::Abi::UserDataMapping::ViewId));
            }
            else if (shaderStage2 == ShaderStageGeometry)
            {
                // Act as hardware ES-GS merged shader
                const auto& entryArgIdxs2 = pIntfData2->entryArgIdxs;

                LLPC_ASSERT((entryArgIdxs1.vs.viewIndex > 0) && (entryArgIdxs2.gs.viewIndex > 0));
                LLPC_ASSERT(pIntfData1->userDataUsage.vs.viewIndex == pIntfData2->userDataUsage.gs.viewIndex);
                SET_DYN_REG(pConfig,
                            startUserData + pIntfData1->userDataUsage.vs.viewIndex,
                            static_cast<uint32_t>(Util::Abi::UserDataMapping::ViewId));
            }
            else
            {
                LLPC_NEVER_CALLED();
            }
        }
    }
    else if (shaderStage1 == ShaderStageTessEval)
    {
        if (enableMultiView)
        {
            if (shaderStage2 == ShaderStageInvalid)
            {
                // Act as hardware VS
                LLPC_ASSERT(entryArgIdxs1.tes.viewIndex > 0);
                SET_DYN_REG(pConfig,
                            startUserData + pIntfData1->userDataUsage.tes.viewIndex,
                            static_cast<uint32_t>(Util::Abi::UserDataMapping::ViewId));
            }
            else if (shaderStage2 == ShaderStageGeometry)
            {
                // Act as hardware ES-GS merged shader
                const auto& entryArgIdxs2 = pIntfData2->entryArgIdxs;

                LLPC_ASSERT((entryArgIdxs1.tes.viewIndex > 0) && (entryArgIdxs2.gs.viewIndex > 0));
                LLPC_ASSERT(pIntfData1->userDataUsage.tes.viewIndex == pIntfData2->userDataUsage.gs.viewIndex);
                SET_DYN_REG(pConfig,
                            startUserData + pIntfData1->userDataUsage.tes.viewIndex,
                            static_cast<uint32_t>(Util::Abi::UserDataMapping::ViewId));
            }
        }
    }
    else if (shaderStage1 == ShaderStageGeometry)
    {
        LLPC_ASSERT(shaderStage2 == ShaderStageInvalid);

        if (enableMultiView)
        {
            LLPC_ASSERT(entryArgIdxs1.gs.viewIndex > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.gs.viewIndex,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::ViewId));
        }
    }
    else if (shaderStage1 == ShaderStageCompute)
    {
        LLPC_ASSERT(shaderStage2 == ShaderStageInvalid);

        if (builtInUsage1.cs.numWorkgroups > 0)
        {
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.cs.numWorkgroupsPtr,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::Workgroup));
        }
    }

    // NOTE: After user data nodes are merged together, any stage of merged shader are ought to have the same
    // configuration for general user data (apart from those special). In this sense, we are safe to use the first
    // shader stage to build user data register settings here.
    SET_DYN_REG(pConfig, startUserData, static_cast<uint32_t>(Util::Abi::UserDataMapping::GlobalTable));

    if (pResUsage1->perShaderTable)
    {
        SET_DYN_REG(pConfig, startUserData + 1, static_cast<uint32_t>(Util::Abi::UserDataMapping::PerShaderTable));
    }

    // NOTE: For copy shader, we use fixed number of user data SGPRs. Thus, there is no need of building user data
    // registers here.
    if (shaderStage1 != ShaderStageCopyShader)
    {
        uint32_t userDataLimit = 0;
        uint32_t spillThreshold = UINT32_MAX;
        uint32_t maxUserDataCount = pContext->GetGpuProperty()->maxUserDataCount;
        for (uint32_t i = 0; i < maxUserDataCount; ++i)
        {
            if (pIntfData1->userDataMap[i] != InterfaceData::UserDataUnmapped)
            {
                SET_DYN_REG(pConfig, startUserData + i, pIntfData1->userDataMap[i]);
                userDataLimit = std::max(userDataLimit, pIntfData1->userDataMap[i] + 1);
            }
        }

        if (pIntfData1->userDataUsage.spillTable > 0)
        {
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData1->userDataUsage.spillTable,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::SpillTable));
            userDataLimit = std::max(userDataLimit,
                                     pIntfData1->spillTable.offsetInDwords + pIntfData1->spillTable.sizeInDwords);
            spillThreshold = pIntfData1->spillTable.offsetInDwords;
        }

        if (userDataLimit > GET_REG(pConfig, USER_DATA_LIMIT))
        {
            SET_REG(pConfig, USER_DATA_LIMIT, userDataLimit)
        }

        if (spillThreshold < GET_REG(pConfig, SPILL_THRESHOLD))
        {
            SET_REG(pConfig, SPILL_THRESHOLD, spillThreshold)
        }
    }

    return result;
}

// =====================================================================================================================
// Sets up the register value for VGT_TF_PARAM.
void ConfigBuilder::SetupVgtTfParam(
    Context*        pContext,  // [in] LLPC context
    LsHsRegConfig*  pConfig)   // [out] Register configuration for local-hull-shader-specific pipeline
{
    uint32_t primType  = InvalidValue;
    uint32_t partition = InvalidValue;
    uint32_t topology  = InvalidValue;

    const auto& builtInUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;

    LLPC_ASSERT(builtInUsage.primitiveMode != SPIRVPrimitiveModeKind::Unknown);
    if (builtInUsage.primitiveMode == Isolines)
    {
        primType = TESS_ISOLINE;
    }
    else if (builtInUsage.primitiveMode == Triangles)
    {
        primType = TESS_TRIANGLE;
    }
    else if (builtInUsage.primitiveMode == Quads)
    {
        primType = TESS_QUAD;
    }
    LLPC_ASSERT(primType != InvalidValue);

    LLPC_ASSERT(builtInUsage.vertexSpacing != SpacingUnknown);
    if (builtInUsage.vertexSpacing == SpacingEqual)
    {
        partition = PART_INTEGER;
    }
    else if (builtInUsage.vertexSpacing == SpacingFractionalOdd)
    {
        partition = PART_FRAC_ODD;
    }
    else if (builtInUsage.vertexSpacing == SpacingFractionalEven)
    {
        partition = PART_FRAC_EVEN;
    }
    LLPC_ASSERT(partition != InvalidValue);

    LLPC_ASSERT(builtInUsage.vertexOrder != VertexOrderUnknown);
    if (builtInUsage.pointMode)
    {
        topology = OUTPUT_POINT;
    }
    else if (builtInUsage.primitiveMode == Isolines)
    {
        topology = OUTPUT_LINE;
    }
    else if (builtInUsage.vertexOrder == VertexOrderCw)
    {
        topology = OUTPUT_TRIANGLE_CW;
    }
    else if (builtInUsage.vertexOrder == VertexOrderCcw)
    {
        topology = OUTPUT_TRIANGLE_CCW;
    }

    auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(pContext->GetPipelineBuildInfo());
    if (pPipelineInfo->iaState.switchWinding)
    {
        if (topology == OUTPUT_TRIANGLE_CW)
        {
            topology = OUTPUT_TRIANGLE_CCW;
        }
        else if (topology == OUTPUT_TRIANGLE_CCW)
        {
            topology = OUTPUT_TRIANGLE_CW;
        }
    }

    LLPC_ASSERT(topology != InvalidValue);

    SET_REG_FIELD(pConfig, VGT_TF_PARAM, TYPE, primType);
    SET_REG_FIELD(pConfig, VGT_TF_PARAM, PARTITIONING, partition);
    SET_REG_FIELD(pConfig, VGT_TF_PARAM, TOPOLOGY, topology);

    if (pContext->IsTessOffChip())
    {
        SET_REG_FIELD(pConfig, VGT_TF_PARAM, DISTRIBUTION_MODE, TRAPEZOIDS);
    }
}

// =====================================================================================================================
// Builds metadata API_HW_SHADER_MAPPING_HI/LO.
void ConfigBuilder::BuildApiHwShaderMapping(
    uint32_t           vsHwShader,    // Hardware shader mapping for vertex shader
    uint32_t           tcsHwShader,   // Hardware shader mapping for tessellation control shader
    uint32_t           tesHwShader,   // Hardware shader mapping for tessellation evaluation shader
    uint32_t           gsHwShader,    // Hardware shader mapping for geometry shader
    uint32_t           fsHwShader,    // Hardware shader mapping for fragment shader
    uint32_t           csHwShader,    // Hardware shader mapping for compute shader
    PipelineRegConfig* pConfig)       // [out] Register configuration for the associated pipeline
{
    Util::Abi::ApiHwShaderMapping apiHwShaderMapping = {};

    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Cs)] = csHwShader;
    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Vs)] = vsHwShader;
    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Hs)] = tcsHwShader;
    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Ds)] = tesHwShader;
    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Gs)] = gsHwShader;
    apiHwShaderMapping.apiShaders[static_cast<uint32_t>(Util::Abi::ApiShaderType::Ps)] = fsHwShader;

    SET_REG(pConfig, API_HW_SHADER_MAPPING_LO, apiHwShaderMapping.u32Lo);
    SET_REG(pConfig, API_HW_SHADER_MAPPING_HI, apiHwShaderMapping.u32Hi);
}

} // Gfx9

} // Llpc
