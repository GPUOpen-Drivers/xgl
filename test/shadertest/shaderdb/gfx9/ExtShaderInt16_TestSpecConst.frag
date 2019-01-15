#version 450

#extension GL_ARB_gpu_shader_int64 : enable
#extension GL_AMD_gpu_shader_int16 : enable

layout(constant_id = 201) const int16_t  i16 = -10s;
layout(constant_id = 202) const uint16_t u16 = 100us;
layout(constant_id = 203) const int      i32 = -20;
layout(constant_id = 204) const uint     u32 = 7;
layout(constant_id = 205) const int64_t  i64 = -67l;
layout(constant_id = 206) const uint64_t u64 = 72ul;

const uint16_t u16_iadd = i16 + u16;
const uint16_t u16_isub = i16 - u16;
const uint16_t u16_imul = i16 * u16;
const int16_t  i16_sdiv = i16 / 2s;
const uint16_t u16_udiv = u16 / 50us;
const int16_t  i16_sneg = -i16;
const int16_t  i16_from_i32 = int16_t(i32);
const int16_t  i16_from_i64 = int16_t(i64);
const uint16_t u16_from_u32 = uint16_t(u32);
const uint16_t u16_from_u64 = uint16_t(u64);
const uint16_t u16_not  = ~u16;
const uint16_t u16_umod = u16 % 9us;
const int16_t  i16_smod = i16 % -3s;
const uint16_t u16_shl  = u16 << u32;
const int16_t  i16_ashr = i16 >> 2;
const uint16_t u16_lshr = u16 >> 5;
const uint16_t u16_or   = u16 | i16;
const uint16_t u16_and  = u16 & i16;
const uint16_t u16_xor  = u16 ^ i16;
const bool     b16_ieq  = (u16 == i16);
const bool     b16_ine  = (i16 != 5);
const bool     b16_ult  = (u16 < 7us);
const bool     b16_slt  = (i16 < 9s);
const bool     b16_ugt  = (u16 > 3us);
const bool     b16_sgt  = (i16 > 4s);
const bool     b16_uge  = (u16 >= 100us);
const bool     b16_sge  = (i16 >= -10s);
const bool     b16_ule  = (u16 <= 100us);
const bool     b16_sle  = (i16 <= -10s);

layout(location = 0) out uint uOut;

void main()
{
    int16_t  i16Data = 0s;
    uint16_t u16Data = 0us;

    u16Data += u16_iadd;
    u16Data += u16_isub;
    u16Data += u16_imul;
    i16Data += i16_sdiv;
    u16Data += u16_udiv;
    i16Data += i16_sneg;
    i16Data += i16_from_i32;
    i16Data += i16_from_i64;
    u16Data += u16_from_u32;
    u16Data += u16_from_u64;
    u16Data += u16_not;
    u16Data += u16_umod;
    i16Data += i16_smod;
    u16Data += u16_shl;
    i16Data += i16_ashr;
    u16Data += u16_lshr;
    u16Data += u16_or;
    u16Data += u16_and;
    u16Data += u16_xor;
    u16Data += uint16_t(b16_ieq);
    u16Data += uint16_t(b16_ine);
    u16Data += uint16_t(b16_ult);
    u16Data += uint16_t(b16_slt);
    u16Data += uint16_t(b16_ugt);
    u16Data += uint16_t(b16_sgt);
    u16Data += uint16_t(b16_uge);
    u16Data += uint16_t(b16_sge);
    u16Data += uint16_t(b16_ule);
    u16Data += uint16_t(b16_sle);

    uOut = u16Data + i16Data;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
