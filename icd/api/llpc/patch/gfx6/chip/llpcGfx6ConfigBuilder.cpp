/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  llpcGfx6ConfigBuilder.cpp
 * @brief LLPC header file: contains implementation of class Llpc::Gfx6::ConfigBuilder.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-gfx6-config-builder"

#include "SPIRVInternal.h"
#include "llpcContext.h"
#include "llpcCodeGenManager.h"
#include "llpcElf.h"
#include "llpcGfx6ConfigBuilder.h"

namespace Llpc
{

namespace Gfx6
{

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-FS).
Result ConfigBuilder::BuildPipelineVsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    const ElfDataEntry* pDataEntries,     // [in] ELF data entries
    void**              ppConfig,         // [out] Register configuration for VS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    PipelineVsFsRegConfig* pConfig = new PipelineVsFsRegConfig();
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderVs,
                            0,
                            0,
                            0,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    if (stageMask & ShaderStageToMask(ShaderStageVertex))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildVsRegConfig<PipelineVsFsRegConfig>(pContext, &dataEntry, ShaderStageVertex, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_REAL);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

        const auto pIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildPsRegConfig<PipelineVsFsRegConfig>(pContext, &dataEntry, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    // NOTE: Align initial settings with those of SCPC.
    const uint32_t primGroupSize = 128;
    iaMultiVgtParam.bits.PRIMGROUP_SIZE     = primGroupSize - 1;
    iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = false;
    iaMultiVgtParam.bits.SWITCH_ON_EOP      = false;

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pConfig;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-TS-FS).
Result ConfigBuilder::BuildPipelineVsTsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    const ElfDataEntry* pDataEntries,     // [in] ELF data entries
    void**              ppConfig,         // [out] Register configuration for VS-TS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;
    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    PipelineVsTsFsRegConfig* pConfig = new PipelineVsTsFsRegConfig();
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderLs,
                            Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderVs,
                            0,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    if (stageMask & ShaderStageToMask(ShaderStageVertex))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildLsRegConfig<PipelineVsTsFsRegConfig>(pContext, &dataEntry, ShaderStageVertex, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, LS_EN, LS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

        const auto pIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageTessControl)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildHsRegConfig<PipelineVsTsFsRegConfig>(pContext, &dataEntry, ShaderStageTessControl, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, HS_EN, HS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessControl);
        SET_REG(pConfig, API_HS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_HS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageTessEval)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildVsRegConfig<PipelineVsTsFsRegConfig>(pContext, &dataEntry, ShaderStageTessEval, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_DS);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessEval);
        SET_REG(pConfig, API_DS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_DS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildPsRegConfig<PipelineVsTsFsRegConfig>(pContext, &dataEntry, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if (pContext->IsTessOffChip())
    {
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, DYNAMIC_HS, true);
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    // NOTE: Align initial settings with those of SCPC.
    const auto& vsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;
    const auto& tcsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;

    if (vsBuiltInUsage.primitiveId || tcsBuiltInUsage.primitiveId)
    {
        iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = true;
        iaMultiVgtParam.bits.SWITCH_ON_EOP = true;
    }

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    // Set up VGT_TF_PARAM
    SetupVgtTfParam<PipelineVsTsFsRegConfig>(pContext, pConfig);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pConfig;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-GS-FS).
Result ConfigBuilder::BuildPipelineVsGsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    const ElfDataEntry* pDataEntries,     // [in] ELF data entries
    void**              ppConfig,         // [out] Register configuration for VS-GS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    PipelineVsGsFsRegConfig* pConfig = new PipelineVsGsFsRegConfig();
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderEs,
                            0,
                            0,
                            Util::Abi::HwShaderGs | Util::Abi::HwShaderVs,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    if (stageMask & ShaderStageToMask(ShaderStageVertex))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildEsRegConfig<PipelineVsGsFsRegConfig>(pContext, &dataEntry, ShaderStageVertex, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, ES_EN, ES_STAGE_REAL);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

        const auto pIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageGeometry)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildGsRegConfig<PipelineVsGsFsRegConfig>(pContext, &dataEntry, ShaderStageGeometry, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, GS_EN, GS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageGeometry);
        SET_REG(pConfig, API_GS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_GS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildPsRegConfig<PipelineVsGsFsRegConfig>(pContext, &dataEntry, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageCopyShader)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildVsRegConfig<PipelineVsGsFsRegConfig>(pContext, &dataEntry, ShaderStageCopyShader, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_COPY_SHADER);
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    // NOTE: Align initial settings with those of SCPC.
    const uint32_t primGroupSize = 128;
    iaMultiVgtParam.bits.PRIMGROUP_SIZE     = primGroupSize - 1;
    iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = false;
    iaMultiVgtParam.bits.SWITCH_ON_EOP      = false;

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pConfig;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for graphics pipeline (VS-TS-GS-FS).
Result ConfigBuilder::BuildPipelineVsTsGsFsRegConfig(
    Context*            pContext,         // [in] LLPC context
    const ElfDataEntry* pDataEntries,     // [in] ELF data entries
    void**              ppConfig,         // [out] Register configuration for VS-TS-GS-FS pipeline
    size_t*             pConfigSize)      // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    uint32_t dataEntryIdx = 0;

    uint64_t hash64 = 0;

    PipelineVsTsGsFsRegConfig* pConfig = new PipelineVsTsGsFsRegConfig();
    pConfig->Init();

    BuildApiHwShaderMapping(Util::Abi::HwShaderLs,
                            Util::Abi::HwShaderHs,
                            Util::Abi::HwShaderEs,
                            Util::Abi::HwShaderGs | Util::Abi::HwShaderVs,
                            Util::Abi::HwShaderPs,
                            0,
                            pConfig);

    if (stageMask & ShaderStageToMask(ShaderStageVertex))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildLsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageVertex, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, LS_EN, LS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageVertex);
        SET_REG(pConfig, API_VS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_VS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

        const auto pIntfData = pContext->GetShaderInterfaceData(ShaderStageVertex);
        if (pIntfData->vbTable.resNodeIdx != InvalidValue)
        {
            SET_REG(pConfig, INDIRECT_TABLE_ENTRY, pIntfData->vbTable.resNodeIdx);
        }
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageTessControl)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildHsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageTessControl, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, HS_EN, HS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessControl);
        SET_REG(pConfig, API_HS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_HS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageTessEval)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildEsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageTessEval, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, ES_EN, ES_STAGE_DS);

        hash64 = pContext->GetShaderHashCode(ShaderStageTessEval);
        SET_REG(pConfig, API_DS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_DS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageGeometry)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildGsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageGeometry, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, GS_EN, GS_STAGE_ON);

        hash64 = pContext->GetShaderHashCode(ShaderStageGeometry);
        SET_REG(pConfig, API_GS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_GS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageFragment)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildPsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageFragment, pConfig);

        hash64 = pContext->GetShaderHashCode(ShaderStageFragment);
        SET_REG(pConfig, API_PS_HASH_LO, static_cast<uint32_t>(hash64));
        SET_REG(pConfig, API_PS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));
    }

    if ((result == Result::Success) && (stageMask & ShaderStageToMask(ShaderStageCopyShader)))
    {
        const auto& dataEntry = pDataEntries[dataEntryIdx++];
        LLPC_ASSERT(dataEntry.size > 0);
        result = BuildVsRegConfig<PipelineVsTsGsFsRegConfig>(pContext, &dataEntry, ShaderStageCopyShader, pConfig);

        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, VS_EN, VS_STAGE_COPY_SHADER);
    }

    if (pContext->IsTessOffChip())
    {
        SET_REG_FIELD(pConfig, VGT_SHADER_STAGES_EN, DYNAMIC_HS, true);
    }

    // Set up IA_MULTI_VGT_PARAM
    regIA_MULTI_VGT_PARAM iaMultiVgtParam = {};

    // NOTE: Align initial settings with those of SCPC.
    const auto& vsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageVertex)->builtInUsage.vs;
    const auto& tcsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageTessControl)->builtInUsage.tcs;
    const auto& gsBuiltInUsage = pContext->GetShaderResourceUsage(ShaderStageGeometry)->builtInUsage.gs;

    if (vsBuiltInUsage.primitiveId || tcsBuiltInUsage.primitiveId || gsBuiltInUsage.primitiveId)
    {
        iaMultiVgtParam.bits.PARTIAL_VS_WAVE_ON = true;
        iaMultiVgtParam.bits.SWITCH_ON_EOP = true;
    }

    SET_REG(pConfig, IA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All);

    // Set up VGT_TF_PARAM
    SetupVgtTfParam<PipelineVsTsGsFsRegConfig>(pContext, pConfig);

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pConfig;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for compute pipeline.
Result ConfigBuilder::BuildPipelineCsRegConfig(
    Context*            pContext,        // [in] LLPC context
    const ElfDataEntry* pDataEntry,      // [in] ELF data entry
    void**              ppConfig,        // [out] Register configuration for compute pipeline
    size_t*             pConfigSize)     // [out] Size of register configuration
{
    Result result = Result::Success;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    LLPC_ASSERT(stageMask == ShaderStageToMask(ShaderStageCompute));

    uint64_t hash64 = 0;

    PipelineCsRegConfig* pConfig = new PipelineCsRegConfig();
    pConfig->Init();

    BuildApiHwShaderMapping(0,
                            0,
                            0,
                            0,
                            0,
                            Util::Abi::HwShaderCs,
                            pConfig);

    LLPC_ASSERT(pDataEntry->size > 0);
    result = BuildCsRegConfig(pContext, pDataEntry, ShaderStageCompute, pConfig);

    hash64 = pContext->GetShaderHashCode(ShaderStageCompute);
    SET_REG(pConfig, API_CS_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, API_CS_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    hash64 = pContext->GetPiplineHashCode();
    SET_REG(pConfig, PIPELINE_HASH_LO, static_cast<uint32_t>(hash64));
    SET_REG(pConfig, PIPELINE_HASH_HI, static_cast<uint32_t>(hash64 >> 32));

    LLPC_ASSERT((ppConfig != nullptr) && (pConfigSize != nullptr));
    *ppConfig = pConfig;
    *pConfigSize = pConfig->GetRegCount() * sizeof(Util::Abi::PalMetadataNoteEntry);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware vertex shader.
template <typename T>
Result ConfigBuilder::BuildVsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for vertex-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT((shaderStage == ShaderStageVertex)   ||
                (shaderStage == ShaderStageTessEval) ||
                (shaderStage == ShaderStageCopyShader));

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    uint32_t dataSize = pDataEntry->size;
    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);

    for (uint32_t i = 0; i < dataSize / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId  = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        CASE_SET_REG(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, regVal)
        CASE_SET_REG(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, regVal)
            break;
        case mmSPI_TMPRING_SIZE * 4:
            spiTmpRingSize.u32All = regVal;
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, DX10_CLAMP, true);  // Follow PAL setting

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, SGPRS));
    uint32_t scratchSize  = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_vsRegs, VS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_vsRegs, VS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_vsRegs, VS_SCRATCH_SIZE, scratchSize);

    SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, USER_SGPR, pIntfData->userDataCount);

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
            // TODO: Need to set SPI_SHADER_PGM_RSRC1_VS.VGPR_COMP_CNT in .AMDGPU.config section
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 0x3); // 0x3: Enable instance ID
        }
        else if (builtInUsage.vs.primitiveId)
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, 0x2);
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

        uint32_t vgtCompCnt = 1;
        if (builtInUsage.tes.tessCoord)
        {
            vgtCompCnt++;
        }

        // NOTE: when primitive ID is used, set vgtCompCnt to 3 directly because primitive ID is the last VGPR.
        if (builtInUsage.tes.primitiveId)
        {
            vgtCompCnt = 3;
        }
        SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC1_VS, VGPR_COMP_CNT, vgtCompCnt);

        if (pContext->IsTessOffChip())
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, SPI_SHADER_PGM_RSRC2_VS, OC_LDS_EN, true);
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

    SET_REG_FIELD(&pConfig->m_vsRegs, VGT_VERTEX_REUSE_BLOCK_CNTL, VTX_REUSE_DEPTH, 14);

    if (usePointSize || useLayer || useViewportIndex)
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_POINT_SIZE, usePointSize);
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_RENDER_TARGET_INDX, useLayer);
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, USE_VTX_VIEWPORT_INDX, useViewportIndex);
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_MISC_VEC_ENA, true);
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_MISC_SIDE_BUS_ENA, true);
    }

    if ((clipDistanceCount > 0) || (cullDistanceCount > 0))
    {
        SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_CCDIST0_VEC_ENA, true);
        if (clipDistanceCount + cullDistanceCount > 4)
        {
            SET_REG_FIELD(&pConfig->m_vsRegs, PA_CL_VS_OUT_CNTL, VS_OUT_CCDIST1_VEC_ENA, true);
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

    if (result == Result::Success)
    {
        // Set shader user data maping
        result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_VS_0, pConfig);
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware hull shader.
template <typename T>
Result ConfigBuilder::BuildHsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for hull-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageTessControl);

    const auto& pIntfData = pContext->GetShaderInterfaceData(shaderStage);
    const auto& calcFactor = pContext->GetShaderResourceUsage(shaderStage)->inOutUsage.tcs.calcFactor;
    const auto& builtInUsage = pContext->GetShaderResourceUsage(shaderStage)->builtInUsage.tcs;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);
    for (uint32_t i = 0; i < pDataEntry->size / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        CASE_SET_REG(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC1_HS, regVal)
        CASE_SET_REG(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC2_HS, regVal)
             break;
        case mmSPI_TMPRING_SIZE * 4:
            spiTmpRingSize.u32All = regVal;
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }
    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC1_HS, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC1_HS, SGPRS));
    uint32_t scratchSize = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_hsRegs, HS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_hsRegs, HS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_hsRegs, HS_SCRATCH_SIZE, scratchSize);

    SET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC1_HS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC1_HS, DX10_CLAMP, true);  // Follow PAL setting

    SET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC2_HS, USER_SGPR, pIntfData->userDataCount);

    if (pContext->IsTessOffChip())
    {
        SET_REG_FIELD(&pConfig->m_hsRegs, SPI_SHADER_PGM_RSRC2_HS, OC_LDS_EN, true);
    }

    // Minimum and maximum tessellation factors supported by the hardware.
    constexpr float MinTessFactor = 1.0f;
    constexpr float MaxTessFactor = 64.0f;
    SET_REG(&pConfig->m_hsRegs, VGT_HOS_MIN_TESS_LEVEL, FloatToBits(MinTessFactor));
    SET_REG(&pConfig->m_hsRegs, VGT_HOS_MAX_TESS_LEVEL, FloatToBits(MaxTessFactor));

    // Set VGT_LS_HS_CONFIG
    SET_REG_FIELD(&pConfig->m_hsRegs, VGT_LS_HS_CONFIG, NUM_PATCHES, calcFactor.patchCountPerThreadGroup);
    auto pPipelineInfo = static_cast<const GraphicsPipelineBuildInfo*>(pContext->GetPipelineBuildInfo());
    SET_REG_FIELD(&pConfig->m_hsRegs, VGT_LS_HS_CONFIG, HS_NUM_INPUT_CP, pPipelineInfo->iaState.patchControlPoints);

    auto hsNumOutputCp = builtInUsage.outputVertices;
    SET_REG_FIELD(&pConfig->m_hsRegs, VGT_LS_HS_CONFIG, HS_NUM_OUTPUT_CP, hsNumOutputCp);

    result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_HS_0, pConfig);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware embedded shader.
template <typename T>
Result ConfigBuilder::BuildEsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for embedded-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT((shaderStage == ShaderStageVertex) || (shaderStage == ShaderStageTessEval));

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage;
    const auto& inOutUsage   = pResUsage->inOutUsage;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    uint32_t dataSize = pDataEntry->size;
    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);

    for (uint32_t i = 0; i < dataSize / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId  = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        case mmSPI_SHADER_PGM_RSRC1_VS * 4:
            // NOTE: backend return SPI_SHADER_PGM_RSRC1_VS for HW ES stage, copy it's value to
            // SPI_SHADER_PGM_RSRC1_ES
            SET_REG(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, regVal);
            break;
        case mmSPI_TMPRING_SIZE * 4:
            // TODO: Support non-zero reg values
            spiTmpRingSize.u32All = regVal;
            if (regVal != 0)
            {
                result = Result::Unsupported;
            }
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    SET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, DX10_CLAMP, true); // Follow PAL setting

    uint32_t vgprCompCnt = 1;
    if (shaderStage == ShaderStageVertex)
    {
        if (builtInUsage.vs.instanceIndex)
        {
            vgprCompCnt = 3; // Enable instance ID
        }
    }
    else
    {
        LLPC_ASSERT(shaderStage == ShaderStageTessEval);

        if (builtInUsage.tes.tessCoord)
        {
            ++vgprCompCnt;
        }

        // NOTE: when primitive ID is used, set vgtCompCnt to 3 directly because primitive ID is the last VGPR.
        if (builtInUsage.tes.primitiveId)
        {
            vgprCompCnt = 3;
        }

        if (pContext->IsTessOffChip())
        {
            SET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC2_ES, OC_LDS_EN, true);
        }
    }

    SET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, VGPR_COMP_CNT, vgprCompCnt);

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC1_ES, SGPRS));
    uint32_t scratchSize  = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_esRegs, ES_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_esRegs, ES_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_esRegs, ES_SCRATCH_SIZE, scratchSize);

    SET_REG_FIELD(&pConfig->m_esRegs, SPI_SHADER_PGM_RSRC2_ES, USER_SGPR, pIntfData->userDataCount);

    uint32_t esGsRingItemSize = 4 * std::max(1u, inOutUsage.outputMapLocCount);
    SET_REG_FIELD(&pConfig->m_esRegs, VGT_ESGS_RING_ITEMSIZE, ITEMSIZE, esGsRingItemSize);

    // Set shader user data maping
    result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_ES_0, pConfig);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware local shader.
template <typename T>
Result ConfigBuilder::BuildLsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for local-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageVertex);

    const auto& pIntfData = pContext->GetShaderInterfaceData(shaderStage);
    const auto& builtInUsage = pContext->GetShaderResourceUsage(shaderStage)->builtInUsage.vs;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);
    for (uint32_t i = 0; i < pDataEntry->size / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        // NOTE: Backend compiler will return SPI_SHADER_PGM_RSRC1/2_VS
        // because the calling convention is amdgpu_vs. So we have to get the
        // value and set it to SPI_SHADER_PGM_RSRC1/2_LS
        case mmSPI_SHADER_PGM_RSRC1_VS * 4:
            SET_REG(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, regVal);
            break;
        case mmSPI_SHADER_PGM_RSRC2_VS * 4:
            SET_REG(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC2_LS, regVal);
            break;
        case mmSPI_TMPRING_SIZE * 4:
            spiTmpRingSize.u32All = regVal;
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, SGPRS));
    uint32_t scratchSize = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_lsRegs, LS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_lsRegs, LS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_lsRegs, LS_SCRATCH_SIZE, scratchSize);

    SET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, DX10_CLAMP, true);  // Follow PAL setting

    uint32_t vgtCompCnt = 1;
    if (builtInUsage.instanceIndex)
    {
        vgtCompCnt += 2; // Enable instance ID
    }
    SET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC1_LS, VGPR_COMP_CNT, vgtCompCnt);

    SET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC2_LS, USER_SGPR, pIntfData->userDataCount);

    const auto& calcFactor = pContext->GetShaderResourceUsage(ShaderStageTessControl)->inOutUsage.tcs.calcFactor;

    uint32_t ldsSizeInDwords = calcFactor.onChip.patchConstStart +
                               calcFactor.patchConstSize * calcFactor.patchCountPerThreadGroup;
    if (pContext->IsTessOffChip())
    {
        ldsSizeInDwords = calcFactor.inPatchSize * calcFactor.patchCountPerThreadGroup;
    }

    uint32_t ldsSize = 0;
    const auto gfxIp = pContext->GetGfxIpVersion();

    if (gfxIp.major == 6)
    {
        // NOTE: On GFX6, granularity for the LDS_SIZE field is 64. The range is 0~128 which allocates 0 to 8K DWORDs.
        const uint32_t ldsSizeDwordGranularity = 64u;
        const uint32_t ldsSizeDwordGranularityShift = 6u;
        ldsSize = Pow2Align(ldsSizeInDwords, ldsSizeDwordGranularity) >> ldsSizeDwordGranularityShift;
    }
    else
    {
        // NOTE: On GFX7+, granularity for the LDS_SIZE field is 128. The range is 0~128 which allocates 0 to 16K
        // DWORDs.
        const uint32_t ldsSizeDwordGranularity = 128u;
        const uint32_t ldsSizeDwordGranularityShift = 7u;
        ldsSize = Pow2Align(ldsSizeInDwords, ldsSizeDwordGranularity) >> ldsSizeDwordGranularityShift;
    }
    SET_REG_FIELD(&pConfig->m_lsRegs, SPI_SHADER_PGM_RSRC2_LS, LDS_SIZE, ldsSize);

    // Set shader user data maping
    result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_LS_0, pConfig);
    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware geometry shader.
template <typename T>
Result ConfigBuilder::BuildGsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for geometry-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageGeometry);

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage.gs;
    const auto& inOutUsage   = pResUsage->inOutUsage;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    uint32_t dataSize = pDataEntry->size;
    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);

    for (uint32_t i = 0; i < dataSize / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId  = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        CASE_SET_REG(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC1_GS, regVal)
        CASE_SET_REG(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC2_GS, regVal)
            break;
        case mmSPI_TMPRING_SIZE * 4:
            // TODO: Support non-zero reg values
            spiTmpRingSize.u32All = regVal;
            if (regVal != 0)
            {
                result = Result::Unsupported;
            }
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    SET_REG_FIELD(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC1_GS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC1_GS, DX10_CLAMP, true);  // Follow PAL setting

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC1_GS, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC1_GS, SGPRS));
    uint32_t scratchSize  = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_gsRegs, GS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_gsRegs, GS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_gsRegs, GS_SCRATCH_SIZE, scratchSize);

    SET_REG_FIELD(&pConfig->m_gsRegs, SPI_SHADER_PGM_RSRC2_GS, USER_SGPR, pIntfData->userDataCount);

    const bool primAdjacency = (builtInUsage.inputPrimitive == InputLinesAdjacency) ||
                               (builtInUsage.inputPrimitive == InputTrianglesAdjacency);

    // Maximum number of GS primitives per ES thread is capped by the hardware's GS-prim FIFO.
    auto pGpuProp = pContext->GetGpuProperty();
    uint32_t maxGsPerEs = (pGpuProp->gsPrimBufferDepth + pGpuProp->waveSize);

    // This limit is halved if the primitive topology is adjacency-typed
    if (primAdjacency)
    {
        maxGsPerEs >>= 1;
    }

    uint32_t maxVertOut = std::max(1u, static_cast<uint32_t>(builtInUsage.outputVertices));
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MAX_VERT_OUT, MAX_VERT_OUT, maxVertOut);

    // TODO: Currently only support offchip GS
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, MODE, GS_SCENARIO_G);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, ONCHIP__CI__VI, VGT_GS_MODE_ONCHIP_OFF);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, ES_WRITE_OPTIMIZE, true);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, GS_WRITE_OPTIMIZE, true);
    if (builtInUsage.outputVertices <= 128)
    {
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_128);
    }
    else if (builtInUsage.outputVertices <= 256)
    {
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_256);
    }
    else if (builtInUsage.outputVertices <= 512)
    {
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_512);
    }
    else
    {
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_MODE, CUT_MODE, GS_CUT_1024);
    }

    SET_REG(&pConfig->m_gsRegs, VGT_GS_ONCHIP_CNTL__CI__VI, 0);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_ES_PER_GS, ES_PER_GS, EsThreadsPerGsThread);
    uint32_t gsVertItemSize = 4 * std::max(1u, inOutUsage.outputMapLocCount);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_VERT_ITEMSIZE, ITEMSIZE, gsVertItemSize);

    if ((builtInUsage.invocations > 1) || builtInUsage.invocationId)
    {
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_INSTANCE_CNT, ENABLE, true);
        SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_INSTANCE_CNT, CNT, builtInUsage.invocations);
    }
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_PER_VS, GS_PER_VS, GsThreadsPerVsThread);

    VGT_GS_OUTPRIM_TYPE gsOutputPrimitiveType = TRISTRIP;
    if (inOutUsage.outputMapLocCount == 0)
    {
        gsOutputPrimitiveType = POINTLIST;
    }
    else if (builtInUsage.outputPrimitive == OutputPoints)
    {
        gsOutputPrimitiveType = POINTLIST;
    }
    else if (builtInUsage.outputPrimitive == LINESTRIP)
    {
        gsOutputPrimitiveType = LINESTRIP;
    }

    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE, gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_1, gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_2, gsOutputPrimitiveType);
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_OUT_PRIM_TYPE, OUTPRIM_TYPE_3, gsOutputPrimitiveType);

    // NOTE: According to register spec,  GSVS_RING_ITEMSIZE must be at least 4 DWORDs.
    uint32_t gsVsRingItemSize = 4 * std::max(1u, inOutUsage.outputMapLocCount * builtInUsage.outputVertices);

    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GSVS_RING_ITEMSIZE, ITEMSIZE, gsVsRingItemSize );

    // TODO: Multiple output streams are not supported.
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GSVS_RING_OFFSET_1, OFFSET, gsVsRingItemSize );
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GSVS_RING_OFFSET_2, OFFSET, gsVsRingItemSize );
    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GSVS_RING_OFFSET_3, OFFSET, gsVsRingItemSize );

    SET_REG_FIELD(&pConfig->m_gsRegs, VGT_GS_PER_ES, GS_PER_ES, std::min(maxGsPerEs, GsPrimsPerEsThread));

    // Set shader user data maping
    result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_GS_0, pConfig);

    return result;
}

// =====================================================================================================================
// Builds register configuration for hardware pixel shader.
template <typename T>
Result ConfigBuilder::BuildPsRegConfig(
    Context*            pContext,       // [in] LLPC context
    const ElfDataEntry* pDataEntry,     // [in] ELF data entry
    ShaderStage         shaderStage,    // Current shader stage (from API side)
    T*                  pConfig)        // [out] Register configuration for pixel-shader-specific pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageFragment);

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage.fs;

    regSPI_TMPRING_SIZE spiTmpRingSize = {};

    uint32_t dataSize = pDataEntry->size;
    const uint32_t* pData = reinterpret_cast<const uint32_t*>(pDataEntry->pData);

    for (uint32_t i = 0; i < dataSize / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId  = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        CASE_SET_REG(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, regVal)
        CASE_SET_REG(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC2_PS, regVal)
        CASE_SET_REG(&pConfig->m_psRegs, SPI_PS_INPUT_ENA, regVal)
        CASE_SET_REG(&pConfig->m_psRegs, SPI_PS_INPUT_ADDR, regVal)
            break;
        case mmSPI_TMPRING_SIZE * 4:
            spiTmpRingSize.u32All = regVal;
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, DX10_CLAMP, true);  // Follow PAL setting
    SET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC2_PS, USER_SGPR, pIntfData->userDataCount);

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_psRegs, SPI_SHADER_PGM_RSRC1_PS, SGPRS));
    uint32_t scratchSize  = GetScratchByteSize(spiTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_psRegs, PS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_psRegs, PS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_psRegs, PS_SCRATCH_SIZE, scratchSize);
    SET_REG(&pConfig->m_psRegs, PS_RUNS_AT_SAMPLE_RATE, builtInUsage.runAtSampleRate);

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
    SET_REG_FIELD(&pConfig->m_psRegs, DB_SHADER_CONTROL, ALPHA_TO_MASK_DISABLE, builtInUsage.sampleMask);
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
        LLPC_ASSERT(((interpInfo[i].loc  == InvalidFsInterpInfo.loc) &&
                        (interpInfo[i].flat == InvalidFsInterpInfo.flat)) == false);

        regSPI_PS_INPUT_CNTL_0 spiPsInputCntl = {};
        spiPsInputCntl.bits.FLAT_SHADE = interpInfo[i].flat;
        spiPsInputCntl.bits.OFFSET = interpInfo[i].loc;

        if (pointCoordLoc == i)
        {
            spiPsInputCntl.bits.PT_SPRITE_TEX = true;
            // NOTE: Set the offset value to force hardware to select input defaults (no VS match).
            constexpr uint32_t UseDefaultVal = (1 << 5);
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

    // NOTE: This is a hardware limitation. Hardware will hang if none of the interpolation mode bits is
    // set. LLVM backend compiler partially work around this issue when none of the interpolation mode bits
    // of SPI_PS_INPUT_ADDR is set. However, the interpolation mode bits of SPI_PS_INPUT_ENA still could be
    // all unset. In the future, when optimization is well done, the register values of SPI_PS_INPUT_ADDR
    // and SPI_PS_INPUT_ENA should be identical
    static const uint32_t InterpModeMask = 0x7F;

    regSPI_PS_INPUT_ADDR spiPsInputAddr = {};
    spiPsInputAddr.u32All = GET_REG(&pConfig->m_psRegs, SPI_PS_INPUT_ADDR);
    LLPC_ASSERT((spiPsInputAddr.u32All & InterpModeMask) != 0);

    regSPI_PS_INPUT_ENA spiPsInputEna = {};
    spiPsInputEna.u32All = GET_REG(&pConfig->m_psRegs, SPI_PS_INPUT_ENA);
    if ((spiPsInputEna.u32All & InterpModeMask) == 0)
    {
        // Always enable PERSP_SAMPLE_ENA if none of those interpolation mode bits is set
        spiPsInputEna.bits.PERSP_SAMPLE_ENA = true;
        SET_REG_FIELD(&pConfig->m_psRegs, SPI_PS_INPUT_ENA, PERSP_SAMPLE_ENA, true);
    }
    LLPC_ASSERT((spiPsInputEna.u32All & InterpModeMask) != 0);

    SET_REG(&pConfig->m_psRegs, PS_USES_UAVS, static_cast<uint32_t>(pResUsage->imageWrite));

    if (result == Result::Success)
    {
        // Set shader user data mapping
        result = ConfigBuilder::BuildUserDataConfig<T>(pContext, shaderStage, mmSPI_SHADER_USER_DATA_PS_0, pConfig);
    }

    return result;
}

// =====================================================================================================================
// Builds register configuration for compute shader.
Result ConfigBuilder::BuildCsRegConfig(
    Context*             pContext,      // [in] LLPC context
    const ElfDataEntry*  pDataEntry,    // [in] ELF data entry
    ShaderStage          shaderStage,   // Current shader stage (from API side)
    PipelineCsRegConfig* pConfig)       // [out] Register configuration for compute pipeline
{
    Result result = Result::Success;

    LLPC_ASSERT(shaderStage == ShaderStageCompute);

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage.cs;

    uint32_t dataSize = pDataEntry[0].size;
    const uint32_t* pData =  reinterpret_cast<const uint32_t*>(pDataEntry[0].pData);

    regCOMPUTE_TMPRING_SIZE computeTmpRingSize = {};

    // Set registers based on ELF section data
    for (uint32_t i = 0; i < dataSize / sizeof(uint32_t); i += 2)
    {
        const uint32_t regId  = pData[i];
        const uint32_t regVal = pData[i + 1];

        switch (regId)
        {
        CASE_SET_REG(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, regVal)
        CASE_SET_REG(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, regVal)
            break;
        case mmSPILLED_SGPRS * 4:
        case mmSPILLED_VGPRS * 4:
            break;
        case mmCOMPUTE_TMPRING_SIZE * 4:
            computeTmpRingSize.u32All = regVal;
            break;
        default:
            LLPC_NEVER_CALLED();
            break;
        }
    }

    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, FLOAT_MODE, 0xC0); // 0xC0: Disable denorm
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, DX10_CLAMP, true);  // Follow PAL setting

    uint32_t numUsedVgprs = GetUsedVgprCount(GET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, VGPRS));
    uint32_t numUsedSgprs = GetUsedSgprCount(GET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC1, SGPRS));
    uint32_t scratchSize  = GetScratchByteSize(computeTmpRingSize.bits.WAVESIZE);

    SET_REG(&pConfig->m_csRegs, CS_NUM_USED_VGPRS, numUsedVgprs);
    SET_REG(&pConfig->m_csRegs, CS_NUM_USED_SGPRS, numUsedSgprs);
    SET_REG(&pConfig->m_csRegs, CS_SCRATCH_SIZE, scratchSize);

    // Set registers based on shader interface data
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, USER_SGPR, pIntfData->userDataCount);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_X_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_Y_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TGID_Z_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TG_SIZE_EN, true);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_PGM_RSRC2, TIDIG_COMP_CNT, (builtInUsage.localInvocationId ? 2 : 0));

    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_X, NUM_THREAD_FULL, builtInUsage.workgroupSizeX);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_Y, NUM_THREAD_FULL, builtInUsage.workgroupSizeY);
    SET_REG_FIELD(&pConfig->m_csRegs, COMPUTE_NUM_THREAD_Z, NUM_THREAD_FULL, builtInUsage.workgroupSizeZ);

    // Set shader user data mapping
    if (result == Result::Success)
    {
        result = ConfigBuilder::BuildUserDataConfig<PipelineCsRegConfig>(pContext,
                                                                         shaderStage,
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
    ShaderStage shaderStage,    // Current shader stage (from API side)
    uint32_t    startUserData,  // Starting user data
    T*          pConfig)        // [out] Register configuration for the associated pipeline
{
    Result result = Result::Success;

    const auto pIntfData = pContext->GetShaderInterfaceData(shaderStage);
    const auto&entryArgIdxs = pIntfData->entryArgIdxs;

    const auto pResUsage = pContext->GetShaderResourceUsage(shaderStage);
    const auto& builtInUsage = pResUsage->builtInUsage;

    const uint32_t stageMask = pContext->GetShaderStageMask();
    bool hasGs = ((stageMask & ShaderStageToMask(ShaderStageGeometry)) != 0);

    // Stage-specific processing
    if (shaderStage == ShaderStageVertex)
    {
        // TODO: PAL only check BaseVertex now, we need update code once PAL check them separately.
        if (builtInUsage.vs.baseVertex || builtInUsage.vs.baseInstance)
        {
            LLPC_ASSERT(entryArgIdxs.vs.baseVertex > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData->userDataUsage.vs.baseVertex,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::BaseVertex));

            LLPC_ASSERT(entryArgIdxs.vs.baseInstance > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData->userDataUsage.vs.baseInstance,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::BaseInstance));
        }

        if (builtInUsage.vs.drawIndex)
        {
            LLPC_ASSERT(entryArgIdxs.vs.drawIndex > 0);
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData->userDataUsage.vs.drawIndex,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::DrawIndex));
        }
    }
    else if (shaderStage == ShaderStageCompute)
    {
        if (builtInUsage.cs.numWorkgroups > 0)
        {
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData->userDataUsage.cs.numWorkgroupsPtr,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::Workgroup));
        }
    }

    SET_DYN_REG(pConfig, startUserData, static_cast<uint32_t>(Util::Abi::UserDataMapping::GlobalTable));

    if (pResUsage->perShaderTable)
    {
        SET_DYN_REG(pConfig, startUserData + 1, static_cast<uint32_t>(Util::Abi::UserDataMapping::PerShaderTable));
    }

    uint32_t userDataLimit = 0;
    uint32_t spillThreshold = UINT32_MAX;
    uint32_t maxUserDataCount = pContext->GetGpuProperty()->maxUserDataCount;
    for (uint32_t i = 0; i < maxUserDataCount; ++i)
    {
        if (pIntfData->userDataMap[i] != InterfaceData::UserDataUnmapped)
        {
            SET_DYN_REG(pConfig, startUserData + i, pIntfData->userDataMap[i]);
            userDataLimit = std::max(userDataLimit, pIntfData->userDataMap[i] + 1);
        }
    }

    if (shaderStage != ShaderStageCopyShader)
    {
        if (pIntfData->userDataUsage.spillTable > 0)
        {
            SET_DYN_REG(pConfig,
                        startUserData + pIntfData->userDataUsage.spillTable,
                        static_cast<uint32_t>(Util::Abi::UserDataMapping::SpillTable));
            userDataLimit = std::max(userDataLimit,
                                     pIntfData->spillTable.offsetInDwords + pIntfData->spillTable.sizeInDwords);
            spillThreshold = pIntfData->spillTable.offsetInDwords;
        }
    }

    if (userDataLimit > GET_REG(pConfig, USER_DATA_LIMIT))
    {
        SET_REG(pConfig, USER_DATA_LIMIT, userDataLimit)
    }

    if (spillThreshold < GET_REG(pConfig, SPILL_THRESHOLD))
    {
        SET_REG(pConfig, SPILL_THRESHOLD, spillThreshold)
    }

    return result;
}

// =====================================================================================================================
// Sets up the register value for VGT_TF_PARAM.
template <typename T>
void ConfigBuilder::SetupVgtTfParam(
    Context* pContext,  // [in] LLPC context
    T*       pConfig)   // [out] Register configuration for the associated pipeline
{
    uint32_t primType  = InvalidValue;
    uint32_t partition = InvalidValue;
    uint32_t topology  = InvalidValue;

    const auto& builtInUsage = pContext->GetShaderResourceUsage(ShaderStageTessEval)->builtInUsage.tes;

    LLPC_ASSERT(builtInUsage.primitiveMode != Unknown);
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
    LLPC_ASSERT(topology != InvalidValue);

    SET_REG_FIELD(pConfig, VGT_TF_PARAM, TYPE, primType);
    SET_REG_FIELD(pConfig, VGT_TF_PARAM, PARTITIONING, partition);
    SET_REG_FIELD(pConfig, VGT_TF_PARAM, TOPOLOGY, topology);
}

// =====================================================================================================================
// Gets the count of used VGPRs from register field PGM_RSRC1.VGPR.
uint32_t ConfigBuilder::GetUsedVgprCount(
    uint32_t regVal)  // Register field value of PGM_RSRC1.VGPR
{
    return (regVal + 1) * 4;
}

// =====================================================================================================================
// Gets the count of used VGPRs from register field PGM_RSRC1.SGPR.
uint32_t ConfigBuilder::GetUsedSgprCount(
    uint32_t regVal)  // Register field value of PGM_RSRC1.SGPR
{
    return (regVal + 1) * 8;
}

// =====================================================================================================================
// Gets the BYTE size of scratch buffer per thread
uint32_t ConfigBuilder::GetScratchByteSize(
    uint32_t regVal) // Register field value of TMPRING_SIZE.WAVESIZE
{
    constexpr uint32_t WaveSizeGranularityShift = 8;
    constexpr uint32_t WavefrontSize = 64;
    return (regVal << WaveSizeGranularityShift) / WavefrontSize * sizeof(uint32_t);
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

} // Gfx6

} // Llpc
