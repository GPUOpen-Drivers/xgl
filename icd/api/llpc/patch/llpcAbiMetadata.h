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
 * @file  llpcAbiMetadata.h
 * @brief LLPC header file: contains declaration of keys used as PAL ABI metadata
 ***********************************************************************************************************************
 */
#pragma once

#include "palPipelineAbi.h"
#include "llpc.h"
#include "llpcDebug.h"

namespace Llpc
{

// Represents pseudo hardware registers.
struct HwReg
{
    uint32_t u32All;  // 32-bit register value
};

// Defines PAL metadata entries based on the specified name and type of this metadata
#define DEF_META(_name, _type) \
typedef HwReg reg##_name;  \
static const uint32_t mm##_name(Util::Abi::PipelineMetadataBase | \
                                static_cast<uint32_t>(Util::Abi::PipelineMetadataType::_type));

DEF_META(API_VS_HASH_LO, ApiVsHashDword0)
DEF_META(API_VS_HASH_HI, ApiVsHashDword1)
DEF_META(API_HS_HASH_LO, ApiHsHashDword0)
DEF_META(API_HS_HASH_HI, ApiHsHashDword1)
DEF_META(API_DS_HASH_LO, ApiDsHashDword0)
DEF_META(API_DS_HASH_HI, ApiDsHashDword1)
DEF_META(API_GS_HASH_LO, ApiGsHashDword0)
DEF_META(API_GS_HASH_HI, ApiGsHashDword1)
DEF_META(API_PS_HASH_LO, ApiPsHashDword0)
DEF_META(API_PS_HASH_HI, ApiPsHashDword1)
DEF_META(API_CS_HASH_LO, ApiCsHashDword0)
DEF_META(API_CS_HASH_HI, ApiCsHashDword1)
DEF_META(PIPELINE_HASH_LO, PipelineHashLo)
DEF_META(PIPELINE_HASH_HI, PipelineHashHi)
DEF_META(USER_DATA_LIMIT, UserDataLimit)
DEF_META(HS_MAX_TESS_FACTOR, HsMaxTessFactor)
DEF_META(PS_USES_UAVS, PsUsesUavs)
DEF_META(PS_USES_ROVS, PsUsesRovs)
DEF_META(PS_RUNS_AT_SAMPLE_RATE, PsRunsAtSampleRate)
DEF_META(SPILL_THRESHOLD, SpillThreshold)
DEF_META(LS_NUM_USED_VGPRS, LsNumUsedVgprs)
DEF_META(HS_NUM_USED_VGPRS, HsNumUsedVgprs)
DEF_META(ES_NUM_USED_VGPRS, EsNumUsedVgprs)
DEF_META(GS_NUM_USED_VGPRS, GsNumUsedVgprs)
DEF_META(VS_NUM_USED_VGPRS, VsNumUsedVgprs)
DEF_META(PS_NUM_USED_VGPRS, PsNumUsedVgprs)
DEF_META(CS_NUM_USED_VGPRS, CsNumUsedVgprs)
DEF_META(LS_NUM_USED_SGPRS, LsNumUsedSgprs)
DEF_META(HS_NUM_USED_SGPRS, HsNumUsedSgprs)
DEF_META(ES_NUM_USED_SGPRS, EsNumUsedSgprs)
DEF_META(GS_NUM_USED_SGPRS, GsNumUsedSgprs)
DEF_META(VS_NUM_USED_SGPRS, VsNumUsedSgprs)
DEF_META(PS_NUM_USED_SGPRS, PsNumUsedSgprs)
DEF_META(CS_NUM_USED_SGPRS, CsNumUsedSgprs)
DEF_META(LS_SCRATCH_SIZE, LsScratchByteSize)
DEF_META(HS_SCRATCH_SIZE, HsScratchByteSize)
DEF_META(ES_SCRATCH_SIZE, EsScratchByteSize)
DEF_META(GS_SCRATCH_SIZE, GsScratchByteSize)
DEF_META(VS_SCRATCH_SIZE, VsScratchByteSize)
DEF_META(PS_SCRATCH_SIZE, PsScratchByteSize)
DEF_META(CS_SCRATCH_SIZE, CsScratchByteSize)
DEF_META(INDIRECT_TABLE_ENTRY, IndirectTableEntryLow)
DEF_META(USES_VIEWPORT_ARRAY_INDEX, UsesViewportArrayIndex)
DEF_META(API_HW_SHADER_MAPPING_LO, ApiHwShaderMappingLo)
DEF_META(API_HW_SHADER_MAPPING_HI, ApiHwShaderMappingHi)

#define mmIA_MULTI_VGT_PARAM_DEFAULT \
    (mmIA_MULTI_VGT_PARAM | \
     static_cast<uint32_t>(Util::Abi::PipelineRegisterFlags::IaMultiVgtParamDefault))

#define mmIA_MULTI_VGT_PARAM_FORCE_SWITCH_ON_EOP \
    (mmIA_MULTI_VGT_PARAM | \
     static_cast<uint32_t>(Util::Abi::PipelineRegisterFlags::IaMultiVgtParamForceSwitchOnEop))

#ifdef LLPC_BUILD_GFX9
#define mmIA_MULTI_VGT_PARAM_DEFAULT__GFX09 \
    (mmIA_MULTI_VGT_PARAM__GFX09 | \
     static_cast<uint32_t>(Util::Abi::PipelineRegisterFlags::IaMultiVgtParamDefault))

#define mmIA_MULTI_VGT_PARAM_FORCE_SWITCH_ON_EOP__GFX09 \
    (mmIA_MULTI_VGT_PARAM__GFX09 | \
     static_cast<uint32_t>(Util::Abi::PipelineRegisterFlags::IaMultiVgtParamForceSwitchOnEop))
#endif

typedef HwReg regIA_MULTI_VGT_PARAM_DEFAULT;
typedef HwReg regIA_MULTI_VGT_PARAM_FORCE_SWITCH_ON_EOP;

} // Llpc
