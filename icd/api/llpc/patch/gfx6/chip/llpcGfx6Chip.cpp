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
 * @file  llpcGfx6Chip.cpp
 * @brief LLPC header file: contains implementations for Gfx6 chips.
 ***********************************************************************************************************************
 */
#define DEBUG_TYPE "llpc-gfx6-chip"

#include "llpcGfx6Chip.h"

namespace Llpc
{

namespace Gfx6
{

#include "si_ci_vi_merged_enum.h"
#include "si_ci_vi_merged_offset.h"

// =====================================================================================================================
// Initializer
void VsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_VS);
    INIT_REG(SPI_SHADER_PGM_RSRC2_VS);
    INIT_REG(SPI_SHADER_POS_FORMAT);
    INIT_REG(SPI_VS_OUT_CONFIG);
    INIT_REG(PA_CL_VS_OUT_CNTL);
    INIT_REG(PA_CL_CLIP_CNTL);
    INIT_REG(PA_CL_VTE_CNTL);
    INIT_REG(PA_SU_VTX_CNTL);
    INIT_REG(VGT_PRIMITIVEID_EN);
    //INIT_REG(VGT_STRMOUT_CONFIG);
    INIT_REG(VGT_STRMOUT_BUFFER_CONFIG);
    INIT_REG(VGT_STRMOUT_VTX_STRIDE_0);
    INIT_REG(VGT_STRMOUT_VTX_STRIDE_1);
    INIT_REG(VGT_STRMOUT_VTX_STRIDE_2);
    INIT_REG(VGT_STRMOUT_VTX_STRIDE_3);
    INIT_REG(VGT_REUSE_OFF);
    INIT_REG(VGT_VERTEX_REUSE_BLOCK_CNTL);
    INIT_REG(VS_SCRATCH_BYTE_SIZE);
    INIT_REG(VS_NUM_USED_VGPRS);
    INIT_REG(VS_NUM_USED_SGPRS);
    INIT_REG(VS_NUM_AVAIL_VGPRS);
    INIT_REG(VS_NUM_AVAIL_SGPRS);
    INIT_REG(USES_VIEWPORT_ARRAY_INDEX);
}

// =====================================================================================================================
// Initializer
void HsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_HS);
    INIT_REG(SPI_SHADER_PGM_RSRC2_HS);
    INIT_REG(HS_SCRATCH_BYTE_SIZE);
    INIT_REG(HS_NUM_USED_VGPRS);
    INIT_REG(HS_NUM_USED_SGPRS);
    INIT_REG(HS_NUM_AVAIL_VGPRS);
    INIT_REG(HS_NUM_AVAIL_SGPRS);
    INIT_REG(VGT_LS_HS_CONFIG);
    INIT_REG(VGT_HOS_MIN_TESS_LEVEL);
    INIT_REG(VGT_HOS_MAX_TESS_LEVEL);
}

// =====================================================================================================================
// Initializer
void EsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_ES);
    INIT_REG(SPI_SHADER_PGM_RSRC2_ES);
    INIT_REG(ES_SCRATCH_BYTE_SIZE);
    INIT_REG(ES_NUM_USED_VGPRS);
    INIT_REG(ES_NUM_USED_SGPRS);
    INIT_REG(ES_NUM_AVAIL_VGPRS);
    INIT_REG(ES_NUM_AVAIL_SGPRS);
    INIT_REG(VGT_ESGS_RING_ITEMSIZE);
}

// =====================================================================================================================
// Initializer
void LsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_LS);
    INIT_REG(SPI_SHADER_PGM_RSRC2_LS);
    INIT_REG(LS_SCRATCH_BYTE_SIZE);
    INIT_REG(LS_NUM_USED_VGPRS);
    INIT_REG(LS_NUM_USED_SGPRS);
    INIT_REG(LS_NUM_AVAIL_VGPRS);
    INIT_REG(LS_NUM_AVAIL_SGPRS);
}

// =====================================================================================================================
// Initializer
void GsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_GS);
    INIT_REG(SPI_SHADER_PGM_RSRC2_GS);
    INIT_REG(GS_SCRATCH_BYTE_SIZE);
    INIT_REG(GS_NUM_USED_VGPRS);
    INIT_REG(GS_NUM_USED_SGPRS);
    INIT_REG(GS_NUM_AVAIL_VGPRS);
    INIT_REG(GS_NUM_AVAIL_SGPRS);
    INIT_REG(VGT_GS_MAX_VERT_OUT);
    INIT_REG(VGT_GS_ONCHIP_CNTL__CI__VI);
    INIT_REG(VGT_ES_PER_GS);
    INIT_REG(VGT_GS_VERT_ITEMSIZE);
    INIT_REG(VGT_GS_INSTANCE_CNT);
    INIT_REG(VGT_GS_PER_VS);
    INIT_REG(VGT_GS_OUT_PRIM_TYPE);
    INIT_REG(VGT_GSVS_RING_ITEMSIZE);
    INIT_REG(VGT_GS_PER_ES);
    INIT_REG(VGT_GS_VERT_ITEMSIZE_1);
    INIT_REG(VGT_GS_VERT_ITEMSIZE_2);
    INIT_REG(VGT_GS_VERT_ITEMSIZE_3);
    INIT_REG(VGT_GSVS_RING_OFFSET_1);
    INIT_REG(VGT_GSVS_RING_OFFSET_2);
    INIT_REG(VGT_GSVS_RING_OFFSET_3);
    INIT_REG(VGT_GS_MODE);
}

// =====================================================================================================================
// Initializer
void PsRegConfig::Init()
{
    INIT_REG(SPI_SHADER_PGM_RSRC1_PS);
    INIT_REG(SPI_SHADER_PGM_RSRC2_PS);
    INIT_REG(SPI_SHADER_Z_FORMAT);
    INIT_REG(SPI_SHADER_COL_FORMAT);
    INIT_REG(SPI_BARYC_CNTL);
    INIT_REG(SPI_PS_IN_CONTROL);
    INIT_REG(SPI_PS_INPUT_ENA);
    INIT_REG(SPI_PS_INPUT_ADDR);
    INIT_REG(SPI_INTERP_CONTROL_0);
    INIT_REG(PA_SC_MODE_CNTL_1);
    INIT_REG(DB_SHADER_CONTROL);
    INIT_REG(CB_SHADER_MASK);
    INIT_REG(PS_USES_UAVS);
    INIT_REG(PS_SCRATCH_BYTE_SIZE);
    INIT_REG(PS_NUM_USED_VGPRS);
    INIT_REG(PS_NUM_USED_SGPRS);
    INIT_REG(PS_NUM_AVAIL_VGPRS);
    INIT_REG(PS_NUM_AVAIL_SGPRS);
}

// =====================================================================================================================
// Initializer
void PipelineRegConfig::Init()
{
    INIT_REG(USER_DATA_LIMIT);
    INIT_REG(SPILL_THRESHOLD);
    INIT_REG(PIPELINE_HASH_LO);
    INIT_REG(PIPELINE_HASH_HI);
    INIT_REG(API_HW_SHADER_MAPPING_LO);
    INIT_REG(API_HW_SHADER_MAPPING_HI);
    SET_REG(this, SPILL_THRESHOLD, UINT32_MAX);
}

// =====================================================================================================================
// Initializer
void PipelineVsFsRegConfig::Init()
{
    m_vsRegs.Init();
    m_psRegs.Init();
    PipelineRegConfig::Init();

    INIT_REG(VGT_SHADER_STAGES_EN);
    INIT_REG(API_VS_HASH_DWORD0);
    INIT_REG(API_VS_HASH_DWORD1);
    INIT_REG(API_PS_HASH_DWORD0);
    INIT_REG(API_PS_HASH_DWORD1);
    INIT_REG(INDIRECT_TABLE_ENTRY);
    INIT_REG(IA_MULTI_VGT_PARAM);

    m_dynRegCount = 0;
}

// =====================================================================================================================
// Initializer
void PipelineVsTsFsRegConfig::Init()
{
    m_lsRegs.Init();
    m_hsRegs.Init();
    m_vsRegs.Init();
    m_psRegs.Init();
    PipelineRegConfig::Init();

    INIT_REG(VGT_SHADER_STAGES_EN);
    INIT_REG(API_VS_HASH_DWORD0);
    INIT_REG(API_VS_HASH_DWORD1);
    INIT_REG(API_HS_HASH_DWORD0);
    INIT_REG(API_HS_HASH_DWORD1);
    INIT_REG(API_DS_HASH_DWORD0);
    INIT_REG(API_DS_HASH_DWORD1);
    INIT_REG(API_PS_HASH_DWORD0);
    INIT_REG(API_PS_HASH_DWORD1);
    INIT_REG(INDIRECT_TABLE_ENTRY);
    INIT_REG(IA_MULTI_VGT_PARAM);
    INIT_REG(VGT_TF_PARAM);

    m_dynRegCount = 0;
}

// =====================================================================================================================
// Initializer
void PipelineVsGsFsRegConfig::Init()
{
    m_esRegs.Init();
    m_gsRegs.Init();
    m_psRegs.Init();
    m_vsRegs.Init();
    PipelineRegConfig::Init();

    INIT_REG(VGT_SHADER_STAGES_EN);
    INIT_REG(API_VS_HASH_DWORD0);
    INIT_REG(API_VS_HASH_DWORD1);
    INIT_REG(API_GS_HASH_DWORD0);
    INIT_REG(API_GS_HASH_DWORD1);
    INIT_REG(API_PS_HASH_DWORD0);
    INIT_REG(API_PS_HASH_DWORD1);
    INIT_REG(INDIRECT_TABLE_ENTRY);
    INIT_REG(IA_MULTI_VGT_PARAM);

    m_dynRegCount = 0;
}

// =====================================================================================================================
// Initializer
void PipelineVsTsGsFsRegConfig::Init()
{
    m_lsRegs.Init();
    m_hsRegs.Init();
    m_esRegs.Init();
    m_gsRegs.Init();
    m_psRegs.Init();
    m_vsRegs.Init();
    PipelineRegConfig::Init();

    INIT_REG(VGT_SHADER_STAGES_EN);
    INIT_REG(API_VS_HASH_DWORD0);
    INIT_REG(API_VS_HASH_DWORD1);
    INIT_REG(API_HS_HASH_DWORD0);
    INIT_REG(API_HS_HASH_DWORD1);
    INIT_REG(API_DS_HASH_DWORD0);
    INIT_REG(API_DS_HASH_DWORD1);
    INIT_REG(API_GS_HASH_DWORD0);
    INIT_REG(API_GS_HASH_DWORD1);
    INIT_REG(API_PS_HASH_DWORD0);
    INIT_REG(API_PS_HASH_DWORD1);
    INIT_REG(INDIRECT_TABLE_ENTRY);
    INIT_REG(IA_MULTI_VGT_PARAM);
    INIT_REG(VGT_TF_PARAM);

    m_dynRegCount = 0;
}

// =====================================================================================================================
// Initializer
void CsRegConfig::Init()
{
    INIT_REG(COMPUTE_PGM_RSRC1);
    INIT_REG(COMPUTE_PGM_RSRC2);
    INIT_REG(COMPUTE_NUM_THREAD_X);
    INIT_REG(COMPUTE_NUM_THREAD_Y);
    INIT_REG(COMPUTE_NUM_THREAD_Z);
    INIT_REG(CS_SCRATCH_BYTE_SIZE);
    INIT_REG(CS_NUM_USED_VGPRS);
    INIT_REG(CS_NUM_USED_SGPRS);
    INIT_REG(CS_NUM_AVAIL_VGPRS);
    INIT_REG(CS_NUM_AVAIL_SGPRS);
}

// =====================================================================================================================
// Initializer
void PipelineCsRegConfig::Init()
{
    m_csRegs.Init();
    PipelineRegConfig::Init();

    INIT_REG(API_CS_HASH_DWORD0);
    INIT_REG(API_CS_HASH_DWORD1);

    m_dynRegCount = 0;
}

// =====================================================================================================================
// Adds entries to register name map.
void InitRegisterNameMap(
    GfxIpVersion gfxIp) // Graphics IP version info
{
    LLPC_ASSERT(gfxIp.major <= 8);

    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_VS);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_VS);
    ADD_REG_MAP(SPI_SHADER_POS_FORMAT);
    ADD_REG_MAP(SPI_VS_OUT_CONFIG);
    ADD_REG_MAP(PA_CL_VS_OUT_CNTL);
    ADD_REG_MAP(PA_CL_CLIP_CNTL);
    ADD_REG_MAP(PA_CL_VTE_CNTL);
    ADD_REG_MAP(PA_SU_VTX_CNTL);
    ADD_REG_MAP(PA_SC_MODE_CNTL_1);
    ADD_REG_MAP(VGT_PRIMITIVEID_EN);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_LS);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_LS);

    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_HS);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_HS);

    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_ES);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_ES);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_GS);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_GS);

    ADD_REG_MAP(VGT_GS_MAX_VERT_OUT);
    ADD_REG_MAP(VGT_ESGS_RING_ITEMSIZE);
    ADD_REG_MAP(VGT_GS_MODE);
    ADD_REG_MAP(VGT_GS_ONCHIP_CNTL__CI__VI);
    ADD_REG_MAP(VGT_ES_PER_GS);
    ADD_REG_MAP(VGT_GS_VERT_ITEMSIZE);
    ADD_REG_MAP(VGT_GS_VERT_ITEMSIZE_1);
    ADD_REG_MAP(VGT_GS_VERT_ITEMSIZE_2);
    ADD_REG_MAP(VGT_GS_VERT_ITEMSIZE_3);
    ADD_REG_MAP(VGT_GSVS_RING_OFFSET_1);
    ADD_REG_MAP(VGT_GSVS_RING_OFFSET_2);
    ADD_REG_MAP(VGT_GSVS_RING_OFFSET_3);

    ADD_REG_MAP(VGT_GS_INSTANCE_CNT);
    ADD_REG_MAP(VGT_GS_PER_VS);
    ADD_REG_MAP(VGT_GS_OUT_PRIM_TYPE);
    ADD_REG_MAP(VGT_GSVS_RING_ITEMSIZE);
    ADD_REG_MAP(VGT_GS_PER_ES);

    ADD_REG_MAP(COMPUTE_PGM_RSRC1);
    ADD_REG_MAP(COMPUTE_PGM_RSRC2);
    ADD_REG_MAP(COMPUTE_TMPRING_SIZE);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC1_PS);
    ADD_REG_MAP(SPI_SHADER_PGM_RSRC2_PS);
    ADD_REG_MAP(SPI_PS_INPUT_ENA);
    ADD_REG_MAP(SPI_PS_INPUT_ADDR);
    ADD_REG_MAP(SPI_INTERP_CONTROL_0);
    ADD_REG_MAP(SPI_TMPRING_SIZE);
    ADD_REG_MAP(SPI_SHADER_Z_FORMAT);
    ADD_REG_MAP(SPI_SHADER_COL_FORMAT);
    ADD_REG_MAP(DB_SHADER_CONTROL);
    ADD_REG_MAP(CB_SHADER_MASK);
    ADD_REG_MAP(SPI_PS_IN_CONTROL);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_0);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_1);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_2);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_3);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_4);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_5);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_6);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_7);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_8);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_9);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_10);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_11);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_12);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_13);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_14);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_15);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_16);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_17);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_18);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_19);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_20);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_21);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_22);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_23);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_24);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_25);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_26);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_27);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_28);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_29);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_30);
    ADD_REG_MAP(SPI_PS_INPUT_CNTL_31);

    ADD_REG_MAP(VGT_SHADER_STAGES_EN);
    ADD_REG_MAP(VGT_VERTEX_REUSE_BLOCK_CNTL);
    ADD_REG_MAP(VGT_STRMOUT_CONFIG);
    ADD_REG_MAP(VGT_STRMOUT_BUFFER_CONFIG);
    ADD_REG_MAP(VGT_STRMOUT_VTX_STRIDE_0);
    ADD_REG_MAP(VGT_STRMOUT_VTX_STRIDE_1);
    ADD_REG_MAP(VGT_STRMOUT_VTX_STRIDE_2);
    ADD_REG_MAP(VGT_STRMOUT_VTX_STRIDE_3);
    ADD_REG_MAP(VGT_REUSE_OFF);

    ADD_REG_MAP(SPI_BARYC_CNTL);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_VS_15);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_HS_15);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_ES_15);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_LS_15);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_GS_15);

    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_0);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_1);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_2);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_3);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_4);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_5);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_6);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_7);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_8);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_9);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_10);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_11);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_12);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_13);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_14);
    ADD_REG_MAP(SPI_SHADER_USER_DATA_PS_15);

    ADD_REG_MAP(COMPUTE_USER_DATA_0);
    ADD_REG_MAP(COMPUTE_USER_DATA_1);
    ADD_REG_MAP(COMPUTE_USER_DATA_2);
    ADD_REG_MAP(COMPUTE_USER_DATA_3);
    ADD_REG_MAP(COMPUTE_USER_DATA_4);
    ADD_REG_MAP(COMPUTE_USER_DATA_5);
    ADD_REG_MAP(COMPUTE_USER_DATA_6);
    ADD_REG_MAP(COMPUTE_USER_DATA_7);
    ADD_REG_MAP(COMPUTE_USER_DATA_8);
    ADD_REG_MAP(COMPUTE_USER_DATA_9);
    ADD_REG_MAP(COMPUTE_USER_DATA_10);
    ADD_REG_MAP(COMPUTE_USER_DATA_11);
    ADD_REG_MAP(COMPUTE_USER_DATA_12);
    ADD_REG_MAP(COMPUTE_USER_DATA_13);
    ADD_REG_MAP(COMPUTE_USER_DATA_14);
    ADD_REG_MAP(COMPUTE_USER_DATA_15);

    ADD_REG_MAP(COMPUTE_NUM_THREAD_X);
    ADD_REG_MAP(COMPUTE_NUM_THREAD_Y);
    ADD_REG_MAP(COMPUTE_NUM_THREAD_Z);

    ADD_REG_MAP(VGT_TF_PARAM);
    ADD_REG_MAP(VGT_LS_HS_CONFIG);
    ADD_REG_MAP(VGT_HOS_MIN_TESS_LEVEL);
    ADD_REG_MAP(VGT_HOS_MAX_TESS_LEVEL);
    ADD_REG_MAP(IA_MULTI_VGT_PARAM);
}

// =====================================================================================================================
// Gets the name string from byte-based ID of the register
const char* GetRegisterNameString(
    GfxIpVersion gfxIp, // Graphics IP version info
    uint32_t     regId) // ID (byte-based) of the register
{
    LLPC_ASSERT(gfxIp.major <= 8);

    if (RegNameMap.empty())
    {
        InitRegisterNameMap(gfxIp);
    }

    const char* pNameString = nullptr;
    if ((regId / 4 >= Util::Abi::PipelineMetadataBase) &&
        (regId / 4 <= Util::Abi::PipelineMetadataBase + static_cast<uint32_t>(Util::Abi::PipelineMetadataType::Count)))
    {
        pNameString = Util::Abi::PipelineMetadataNameStrings[regId / 4 - Util::Abi::PipelineMetadataBase];
    }
    else
    {
        auto nameMap = RegNameMap.find(regId);
        if  (nameMap != RegNameMap.end())
        {
            pNameString = nameMap->second;
        }
        else
        {
            static char unknownRegNameBuf[256] = {};
            int32_t length = snprintf(unknownRegNameBuf, 256, "UNKNOWN(0x%08X)", regId);
            pNameString = unknownRegNameBuf;
        }
    }

    return pNameString;
}

} // Gfx6

} // Llpc
