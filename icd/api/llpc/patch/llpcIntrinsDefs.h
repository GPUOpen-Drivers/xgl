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
 * @file  llpcIntrinsDefs.h
 * @brief LLPC header file: contains various definitions used by LLPC AMDGPU-backend instrinics.
 ***********************************************************************************************************************
 */
#pragma once

namespace Llpc
{

// Limits
static const uint32_t MaxTessPatchVertices              = (1 << 6)  - 1;

static const uint32_t MaxGeometryInvocations            = (1 << 7)  - 1;
static const uint32_t MaxGeometryOutputVertices         = (1 << 11) - 1;

static const uint32_t MaxComputeWorkgroupSize           = (1 << 16) - 1;

// Message type if s_sendmsg
static const uint32_t GS_DONE = 3;            // GS wave is done
static const uint32_t GS_EMIT_STREAM0 = 0x22; // [3:0] = 2 (GS), [5:4] = 2 (emit), [9:8] = 0 (stream0)

// Enumerates address spaces valid for AMD GPU (similar to LLVM header AMDGPU.h)
enum AddrSpace
{
    ADDR_SPACE_GLOBAL           = 1,            // Global memory
    ADDR_SPACE_LOCAL            = 3,            // Local memory
    ADDR_SPACE_CONST            = 4,            // Constant memory
};

// Enumerates the target for "export" instruction.
enum ExportTarget
{
    EXP_TARGET_MRT_0            = 0,            // MRT 0..7
    EXP_TARGET_Z                = 8,            // Z
    EXP_TARGET_PS_NULL          = 9,            // Null pixel shader export (no data)
    EXP_TARGET_POS_0            = 12,           // Position 0
    EXP_TARGET_POS_1            = 13,           // Position 1
    EXP_TARGET_POS_2            = 14,           // Position 2
    EXP_TARGET_POS_3            = 15,           // Position 3
    EXP_TARGET_PARAM_0          = 32,           // Param 0..31
};

// Enumerates shader export format used for "export" instruction.
enum ExportFormat
{
    EXP_FORMAT_ZERO             = 0,            // ZERO
    EXP_FORMAT_32_R             = 1,            // 32_R
    EXP_FORMAT_32_GR            = 2,            // 32_GR
    EXP_FORMAT_32_AR            = 3,            // 32_AR
    EXP_FORMAT_FP16_ABGR        = 4,            // FP16_ABGR
    EXP_FORMAT_UNORM16_ABGR     = 5,            // UNORM16_ABGR
    EXP_FORMAT_SNORM16_ABGR     = 6,            // SNORM16_ABGR
    EXP_FORMAT_UINT16_ABGR      = 7,            // UINT16_ABGR
    EXP_FORMAT_SINT16_ABGR      = 8,            // SINT16_ABGR
    EXP_FORMAT_32_ABGR          = 9,            // 32_ABGR
};

// Enumerates data format of data in CB.
enum ColorDataFormat
{
    COLOR_DATA_FORMAT_INVALID                   = 0,            // Invalid
    COLOR_DATA_FORMAT_8                         = 1,            // 8
    COLOR_DATA_FORMAT_16                        = 2,            // 16
    COLOR_DATA_FORMAT_8_8                       = 3,            // 8_8
    COLOR_DATA_FORMAT_32                        = 4,            // 32
    COLOR_DATA_FORMAT_16_16                     = 5,            // 16_16
    COLOR_DATA_FORMAT_10_11_11                  = 6,            // 10_11_11
    COLOR_DATA_FORMAT_11_11_10                  = 7,            // 11_11_10
    COLOR_DATA_FORMAT_10_10_10_2                = 8,            // 10_10_10_2
    COLOR_DATA_FORMAT_2_10_10_10                = 9,            // 2_10_10_10
    COLOR_DATA_FORMAT_8_8_8_8                   = 10,           // 8_8_8_8
    COLOR_DATA_FORMAT_32_32                     = 11,           // 32_32
    COLOR_DATA_FORMAT_16_16_16_16               = 12,           // 16_16_16_16
    COLOR_DATA_FORMAT_32_32_32_32               = 14,           // 32_32_32_32
    COLOR_DATA_FORMAT_5_6_5                     = 16,           // 5_6_5
    COLOR_DATA_FORMAT_1_5_5_5                   = 17,           // 1_5_5_5
    COLOR_DATA_FORMAT_5_5_5_1                   = 18,           // 5_5_5_1
    COLOR_DATA_FORMAT_4_4_4_4                   = 19,           // 4_4_4_4
    COLOR_DATA_FORMAT_8_24                      = 20,           // 8_24
    COLOR_DATA_FORMAT_24_8                      = 21,           // 24_8
    COLOR_DATA_FORMAT_X24_8_32_FLOAT            = 22,           // X24_8_32_FLOAT
    COLOR_DATA_FORMAT_2_10_10_10_6E4            = 31,           // 2_10_10_10_6E4
};

// Enumerates numeric format of data in CB.
enum ColorNumFormat
{
    COLOR_NUM_FORMAT_UNORM                      = 0,            // UNORM
    COLOR_NUM_FORMAT_SNORM                      = 1,            // SNORM
    COLOR_NUM_FORMAT_USCALED                    = 2,            // USCALED
    COLOR_NUM_FORMAT_SSCALED                    = 3,            // SSCALED
    COLOR_NUM_FORMAT_UINT                       = 4,            // UINT
    COLOR_NUM_FORMAT_SINT                       = 5,            // SINT
    COLOR_NUM_FORMAT_SRGB                       = 6,            // SRGB
    COLOR_NUM_FORMAT_FLOAT                      = 7,            // FLOAT
};

// Enumrates CB component swap mode.
enum ColorSwap
{
    COLOR_SWAP_STD                              = 0,            // STD
    COLOR_SWAP_ALT                              = 1,            // ALT
    COLOR_SWAP_STD_REV                          = 2,            // STD_REV
    COLOR_SWAP_ALT_REV                          = 3,            // ALT_REV
};

// Enumerates parameter values used in "flat" interpolation (v_interp_mov).
enum InterpParam
{
    INTERP_PARAM_P10            = 0,            // P10
    INTERP_PARAM_P20            = 1,            // P20
    INTERP_PARAM_P0             = 2,            // P0
};

// Enumerates data format of data in memory buffer.
enum BufDataFormat
{
    BUF_DATA_FORMAT_INVALID                     = 0,            // Invalid
    BUF_DATA_FORMAT_8                           = 1,            // 8
    BUF_DATA_FORMAT_16                          = 2,            // 16
    BUF_DATA_FORMAT_8_8                         = 3,            // 8_8
    BUF_DATA_FORMAT_32                          = 4,            // 32
    BUF_DATA_FORMAT_16_16                       = 5,            // 16_16
    BUF_DATA_FORMAT_10_11_11                    = 6,            // 10_11_11
    BUF_DATA_FORMAT_11_11_10                    = 7,            // 11_11_10
    BUF_DATA_FORMAT_10_10_10_2                  = 8,            // 10_10_10_2
    BUF_DATA_FORMAT_2_10_10_10                  = 9,            // 2_10_10_10
    BUF_DATA_FORMAT_8_8_8_8                     = 10,           // 8_8_8_8
    BUF_DATA_FORMAT_32_32                       = 11,           // 32_32
    BUF_DATA_FORMAT_16_16_16_16                 = 12,           // 16_16_16_16
    BUF_DATA_FORMAT_32_32_32                    = 13,           // 32_32_32
    BUF_DATA_FORMAT_32_32_32_32                 = 14,           // 32_32_32_32
};

// Enumerates numeric format of data in memory buffer.
enum BufNumFormat
{
    BUF_NUM_FORMAT_UNORM                        = 0,            // Unorm
    BUF_NUM_FORMAT_SNORM                        = 1,            // Snorm
    BUF_NUM_FORMAT_USCALED                      = 2,            // Uscaled
    BUF_NUM_FORMAT_SSCALED                      = 3,            // Sscaled
    BUF_NUM_FORMAT_UINT                         = 4,            // Uint
    BUF_NUM_FORMAT_SINT                         = 5,            // Sint
    BUF_NUM_FORMAT_SNORM_OGL                    = 6,            // Snorm_ogl
    BUF_NUM_FORMAT_FLOAT                        = 7,            // Float
};

// Enumerates destination selection of data in memory buffer.
enum BufDstSel
{
    BUF_DST_SEL_0 = 0,    // SEL_0 (0.0)
    BUF_DST_SEL_1 = 1,    // SEL_1 (1.0)
    BUF_DST_SEL_X = 4,    // SEL_X (X)
    BUF_DST_SEL_Y = 5,    // SEL_Y (Y)
    BUF_DST_SEL_Z = 6,    // SEL_Z (Z)
    BUF_DST_SEL_W = 7,    // SEL_W (W)
};

// Represents register fields of SPI_PS_INPUT_ADDR.
union SpiPsInputAddr
{
    struct
    {
        uint32_t    PERSP_SAMPLE_ENA        : 1;        // PERSP_SAMPLE_ENA
        uint32_t    PERSP_CENTER_ENA        : 1;        // PERSP_CENTER_ENA
        uint32_t    PERSP_CENTROID_ENA      : 1;        // PERSP_CENTROID_ENA
        uint32_t    PERSP_PULL_MODEL_ENA    : 1;        // PERSP_PULL_MODEL_ENA
        uint32_t    LINEAR_SAMPLE_ENA       : 1;        // LINEAR_SAMPLE_ENA
        uint32_t    LINEAR_CENTER_ENA       : 1;        // LINEAR_CENTER_ENA
        uint32_t    LINEAR_CENTROID_ENA     : 1;        // LINEAR_CENTROID_ENA
        uint32_t    LINE_STIPPLE_TEX_ENA    : 1;        // LINE_STIPPLE_TEX_ENA
        uint32_t    POS_X_FLOAT_ENA         : 1;        // POS_X_FLOAT_ENA
        uint32_t    POS_Y_FLOAT_ENA         : 1;        // POS_Y_FLOAT_ENA
        uint32_t    POS_Z_FLOAT_ENA         : 1;        // POS_Z_FLOAT_ENA
        uint32_t    POS_W_FLOAT_ENA         : 1;        // POS_W_FLOAT_ENA
        uint32_t    FRONT_FACE_ENA          : 1;        // FRONT_FACE_ENA
        uint32_t    ANCILLARY_ENA           : 1;        // ANCILLARY_ENA
        uint32_t    SAMPLE_COVERAGE_ENA     : 1;        // SAMPLE_COVERAGE_ENA
        uint32_t    POS_FIXED_PT_ENA        : 1;        // POS_FIXED_PT_ENA

        uint32_t    unused                  : 16;
    };

    uint32_t u32All;
};

// Represents the first word of buffer descriptor SQ_BUF_RSRC_WORD0
union SqBufRsrcWord0 {
    struct {
        uint32_t    BASE_ADDRESS : 32;       // BASE_ADDRESS
    } bits;
    uint32_t u32All;
};

// Represents the second word of buffer descriptor SQ_BUF_RSRC_WORD1
union SqBufRsrcWord1 {
    struct {
        uint32_t    BASE_ADDRESS_HI : 16;    // BASE_ADDRESS_HI
        uint32_t    STRIDE          : 14;    // STRIDE
        uint32_t    CACHE_SWIZZLE   : 1;     // CACHE_SWIZZLE
        uint32_t    SWIZZLE_ENABLE  : 1;     // SWIZZLE_ENABLE
    } bits;
    uint32_t	u32All;
};

// Represents the third word of buffer descriptor SQ_BUF_RSRC_WORD2
union SqBufRsrcWord2 {
    struct {
        uint32_t    NUM_RECORDS : 32;        // NUM_RECORDS
    } bits;
    uint32_t u32All;
};

// Represents the forth word of buffer descriptor SQ_BUF_RSRC_WORD3
union SqBufRsrcWord3 {
    struct {
        uint32_t    DST_SEL_X      : 3;      // DST_SEL_X
        uint32_t    DST_SEL_Y      : 3;      // DST_SEL_Y
        uint32_t    DST_SEL_Z      : 3;      // DST_SEL_Z
        uint32_t    DST_SEL_W      : 3;      // DST_SEL_W
        uint32_t    NUM_FORMAT     : 3;      // NUM_FORMAT
        uint32_t    DATA_FORMAT    : 4;      // DATA_FORMAT
        uint32_t    ELEMENT_SIZE   : 2;      // ELEMENT_SIZE
        uint32_t    INDEX_STRIDE   : 2;      // INDEX_STRIDE
        uint32_t    ADD_TID_ENABLE : 1;      // ADD_TID_ENABLE
        uint32_t    ATC__CI__VI    : 1;      // ATC
        uint32_t    HASH_ENABLE    : 1;      // HASH_ENABLE
        uint32_t    HEAP           : 1;      // HEAP
        uint32_t    MTYPE__CI__VI  : 3;      // MTYPE
        uint32_t    TYPE           : 2;      // TYPE
    } bits;
    uint32_t u32All;
};

} // Llpc
