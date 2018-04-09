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

DEF_META(API_VS_HASH_DWORD0, ApiVsHashDword0)
DEF_META(API_VS_HASH_DWORD1, ApiVsHashDword1)
DEF_META(API_VS_HASH_DWORD2, ApiVsHashDword2)
DEF_META(API_VS_HASH_DWORD3, ApiVsHashDword3)
DEF_META(API_HS_HASH_DWORD0, ApiHsHashDword0)
DEF_META(API_HS_HASH_DWORD1, ApiHsHashDword1)
DEF_META(API_HS_HASH_DWORD2, ApiHsHashDword2)
DEF_META(API_HS_HASH_DWORD3, ApiHsHashDword3)
DEF_META(API_DS_HASH_DWORD0, ApiDsHashDword0)
DEF_META(API_DS_HASH_DWORD1, ApiDsHashDword1)
DEF_META(API_DS_HASH_DWORD2, ApiDsHashDword2)
DEF_META(API_DS_HASH_DWORD3, ApiDsHashDword3)
DEF_META(API_GS_HASH_DWORD0, ApiGsHashDword0)
DEF_META(API_GS_HASH_DWORD1, ApiGsHashDword1)
DEF_META(API_GS_HASH_DWORD2, ApiGsHashDword2)
DEF_META(API_GS_HASH_DWORD3, ApiGsHashDword3)
DEF_META(API_PS_HASH_DWORD0, ApiPsHashDword0)
DEF_META(API_PS_HASH_DWORD1, ApiPsHashDword1)
DEF_META(API_PS_HASH_DWORD2, ApiPsHashDword2)
DEF_META(API_PS_HASH_DWORD3, ApiPsHashDword3)
DEF_META(API_CS_HASH_DWORD0, ApiCsHashDword0)
DEF_META(API_CS_HASH_DWORD1, ApiCsHashDword1)
DEF_META(API_CS_HASH_DWORD2, ApiCsHashDword2)
DEF_META(API_CS_HASH_DWORD3, ApiCsHashDword3)
DEF_META(PIPELINE_HASH_LO, PipelineHashLo)
DEF_META(PIPELINE_HASH_HI, PipelineHashHi)
DEF_META(USER_DATA_LIMIT, UserDataLimit)
DEF_META(HS_MAX_TESS_FACTOR, HsMaxTessFactor)
DEF_META(PS_USES_UAVS, PsUsesUavs)
DEF_META(PS_USES_ROVS, PsUsesRovs)
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
DEF_META(LS_NUM_AVAIL_VGPRS, LsNumAvailableVgprs)
DEF_META(HS_NUM_AVAIL_VGPRS, HsNumAvailableVgprs)
DEF_META(ES_NUM_AVAIL_VGPRS, EsNumAvailableVgprs)
DEF_META(GS_NUM_AVAIL_VGPRS, GsNumAvailableVgprs)
DEF_META(VS_NUM_AVAIL_VGPRS, VsNumAvailableVgprs)
DEF_META(PS_NUM_AVAIL_VGPRS, PsNumAvailableVgprs)
DEF_META(CS_NUM_AVAIL_VGPRS, CsNumAvailableVgprs)
DEF_META(LS_NUM_AVAIL_SGPRS, LsNumAvailableSgprs)
DEF_META(HS_NUM_AVAIL_SGPRS, HsNumAvailableSgprs)
DEF_META(ES_NUM_AVAIL_SGPRS, EsNumAvailableSgprs)
DEF_META(GS_NUM_AVAIL_SGPRS, GsNumAvailableSgprs)
DEF_META(VS_NUM_AVAIL_SGPRS, VsNumAvailableSgprs)
DEF_META(PS_NUM_AVAIL_SGPRS, PsNumAvailableSgprs)
DEF_META(CS_NUM_AVAIL_SGPRS, CsNumAvailableSgprs)
DEF_META(LS_LDS_BYTE_SIZE, LsLdsByteSize)
DEF_META(HS_LDS_BYTE_SIZE, HsLdsByteSize)
DEF_META(ES_LDS_BYTE_SIZE, EsLdsByteSize)
DEF_META(GS_LDS_BYTE_SIZE, GsLdsByteSize)
DEF_META(VS_LDS_BYTE_SIZE, VsLdsByteSize)
DEF_META(PS_LDS_BYTE_SIZE, PsLdsByteSize)
DEF_META(CS_LDS_BYTE_SIZE, CsLdsByteSize)
DEF_META(LS_SCRATCH_BYTE_SIZE, LsScratchByteSize)
DEF_META(HS_SCRATCH_BYTE_SIZE, HsScratchByteSize)
DEF_META(ES_SCRATCH_BYTE_SIZE, EsScratchByteSize)
DEF_META(GS_SCRATCH_BYTE_SIZE, GsScratchByteSize)
DEF_META(VS_SCRATCH_BYTE_SIZE, VsScratchByteSize)
DEF_META(PS_SCRATCH_BYTE_SIZE, PsScratchByteSize)
DEF_META(CS_SCRATCH_BYTE_SIZE, CsScratchByteSize)
DEF_META(STREAM_OUT_TABLE_ENTRY, StreamOutTableEntry)
DEF_META(INDIRECT_TABLE_ENTRY, IndirectTableEntryLow)
DEF_META(ES_GS_LDS_BYTE_SIZE, EsGsLdsByteSize)
DEF_META(USES_VIEWPORT_ARRAY_INDEX, UsesViewportArrayIndex)
DEF_META(PIPELINE_NAME_INDEX, PipelineNameIndex)
DEF_META(API_HW_SHADER_MAPPING_LO, ApiHwShaderMappingLo)
DEF_META(API_HW_SHADER_MAPPING_HI, ApiHwShaderMappingHi)
DEF_META(LS_PERFORMANCE_DATA_BUFFER_SIZE, LsPerformanceDataBufferSize)
DEF_META(HS_PERFORMANCE_DATA_BUFFER_SIZE, HsPerformanceDataBufferSize)
DEF_META(ES_PERFORMANCE_DATA_BUFFER_SIZE, EsPerformanceDataBufferSize)
DEF_META(GS_PERFORMANCE_DATA_BUFFER_SIZE, GsPerformanceDataBufferSize)
DEF_META(VS_PERFORMANCE_DATA_BUFFER_SIZE, VsPerformanceDataBufferSize)
DEF_META(PS_PERFORMANCE_DATA_BUFFER_SIZE, PsPerformanceDataBufferSize)
DEF_META(CS_PERFORMANCE_DATA_BUFFER_SIZE, CsPerformanceDataBufferSize)
} // Llpc
