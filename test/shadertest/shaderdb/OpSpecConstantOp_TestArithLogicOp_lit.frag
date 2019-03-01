#version 450 core

#extension GL_ARB_gpu_shader_int64: enable

layout(constant_id = 0) const float     scf     = 0.4;
layout(constant_id = 1) const double    scd     = 0.5;
layout(constant_id = 2) const int       sci     = -2;
layout(constant_id = 3) const uint      scu     = 3;
layout(constant_id = 4) const bool      scb     = true;
layout(constant_id = 5) const int64_t   sci64   = -6l;
layout(constant_id = 6) const uint64_t  scu64   = 7ul;

// OpSConvert
const int64_t i_to_i64 = int64_t(sci);
const int     i64_to_i = int(sci64);

// OpUConvert
const uint64_t u_to_u64 = uint64_t(scu);
const uint     u64_to_u = uint(scu64);

// OpFConvert
const double f_to_d = double(scf);
const float  d_to_f = float(scd);

// OpSNegate
const int     i_neg   = -sci;
const int64_t i64_neg = -sci64;

// OpNot
const uint     u_not   = ~scu;
const uint64_t u64_not = ~scu64;

// OpIAdd
const int      i_add   = sci + 1;
const uint     u_add   = scu + 1;
const int64_t  i64_add = sci64 + 1;
const uint64_t u64_add = scu64 + 1;

// OpISub
const int      i_sub   = sci - 1;
const uint     u_sub   = scu - 1;
const int64_t  i64_sub = sci64 - 1;
const uint64_t u64_sub = scu64 - 1;

// OpIMul
const int      i_mul   = sci * 3;
const uint     u_mul   = scu * 3;
const int64_t  i64_mul = sci64 * 3;
const uint64_t u64_mul = scu64 * 3;

// OpUDiv
const uint     u_div   = scu / 3;
const uint64_t u64_div = scu64 / 3;

// OpSDiv
const int      i_div   = sci / 3;
const int64_t  i64_div = sci64 / 3;

// OpUMod
const uint     u_mod   = scu % 3;
const uint64_t u64_mod = scu64 % 3;

// OpSMod
const int      i_mod   = sci % 3;
const int64_t  i64_mod = sci64 % 3;

// OpShiftRightLogical
const uint     u_srl   = scu >> 3;
const uint64_t u64_srl = scu64 >> 3;

// OpShiftRightArithmetic
const int      i_sra   = sci >> 3;
const int64_t  i64_sra = sci64 >> 3;

// OpShiftLeftLogical
const int      i_sll   = sci << 3;
const uint     u_sll   = scu << 3;
const int64_t  i64_sll = sci64 << 3;
const uint64_t u64_sll = scu64 << 3;

// OpBitwiseOr
const uint     u_or    = sci | scu;
const uint64_t u64_or  = sci64 | scu64;

// OpBitwiseXor
const uint     u_xor   = sci ^ scu;
const uint64_t u64_xor = sci64 ^ scu64;

// OpBitwiseAnd
const uint     u_and   = sci & scu;
const uint64_t u64_and = sci64 & scu64;

// OpLogicalOr
const bool b_or  = scb || scb;

// OpLogicalAnd
const bool b_and = scb && scb;

// OpLogicalNot
const bool b_not = !scb;

// OpLogicalEqual
const bool b_eq = (scb == true);

// OpLogicalNotEqual
const bool b_ne = (scb != false);

// OpSelect
const int  i_sel = scb ? sci : -9;

// OpIEqual
const bool b_ieq    = (sci == scu);
const bool b_i64eq  = (sci64 == scu64);

// OpINotEqual
const bool b_ine    = (sci != scu);
const bool b_i64ne  = (sci64 != scu64);

// OpULessThan
const bool b_ult    = (scu < 66);
const bool b_u64lt  = (scu64 < 66ul);

// OpSLessThan
const bool b_ilt    = (sci < 66);
const bool b_i64lt  = (sci64 < 66l);

// OpUGreaterThan
const bool b_ugt    = (scu > 66);
const bool b_u64gt  = (scu64 > 66ul);

// OpSGreaterThan
const bool b_igt    = (sci > 66);
const bool b_i64gt  = (sci64 > 66l);

// OpULessThanEqual
const bool b_ule    = (scu <= 66);
const bool b_u64le  = (scu64 <= 66ul);

// OpSLessThanEqual
const bool b_ile    = (sci <= 66);
const bool b_i64le  = (sci64 <= 66l);

// OpUGreaterThanEqual
const bool b_uge    = (scu >= 66);
const bool b_u64ge  = (scu64 >= 66ul);

// OpSGreaterThanEqual
const bool b_ige    = (sci >= 66);
const bool b_i64ge  = (sci64 >= 66l);

layout(location = 0) out vec4 f3;

void main()
{
    float    f   = 0.0;
    double   d   = 0.0;
    int      i   = 0;
    uint     u   = 0;
    bool     b   = false;
    int64_t  i64 = 0;
    uint64_t u64 = 0;

    f += d_to_f;

    d += f_to_d;

    i += i64_to_i;
    i += i_neg;
    i += i_add;
    i += i_sub;
    i += i_mul;
    i += i_div;
    i += i_mod;
    i += i_sra;
    i += i_sll;
    i += i_sel;

    u += u64_to_u;
    u += u_not;
    u += u_add;
    u += u_sub;
    u += u_mul;
    u += u_div;
    u += u_mod;
    u += u_srl;
    u += u_sll;
    u += u_or;
    u += u_xor;
    u += u_and;

    b = b || b_or;
    b = b || b_and;
    b = b || b_not;
    b = b || b_eq;
    b = b || b_ne;
    b = b || b_ieq;
    b = b || b_i64eq;
    b = b || b_ine;
    b = b || b_i64ne;
    b = b || b_ult;
    b = b || b_u64lt; 
    b = b || b_ilt;
    b = b || b_i64lt;
    b = b || b_ugt;
    b = b || b_u64gt;
    b = b || b_igt;
    b = b || b_i64gt;
    b = b || b_ule;
    b = b || b_u64le;
    b = b || b_ile;
    b = b || b_i64le;
    b = b || b_uge;
    b = b || b_u64ge;
    b = b || b_ige;
    b = b || b_i64ge;

    i64 += i_to_i64;
    i64 += i64_neg;
    i64 += i64_add;
    i64 += i64_sub;
    i64 += i64_mul;
    i64 += i64_div;
    i64 += i64_mod;
    i64 += i64_sra;
    i64 += i64_sll;

    u64 += u_to_u64;
    u64 += u64_not;
    u64 += u64_add;
    u64 += u64_sub;
    u64 += u64_mul;
    u64 += u64_div;
    u64 += u64_mod;
    u64 += u64_srl;
    u64 += u64_sll;
    u64 += u64_or;
    u64 += u64_xor;
    u64 += u64_and;

    f += float(d);
    f += float(i);
    f += float(u);
    f += float(b);
    f += float(i64);
    f += float(u64);

    f3 = vec4(f);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fadd reassoc nnan arcp contract float %1, 5.000000e-01
; SHADERTEST: add i32 %{{[0-9]*}}, 2
; SHADERTEST: add i32 %{{[0-9]*}}, -1
; SHADERTEST: add i32 %{{[0-9]*}}, -3
; SHADERTEST: add i32 %{{[0-9]*}}, -6


; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
