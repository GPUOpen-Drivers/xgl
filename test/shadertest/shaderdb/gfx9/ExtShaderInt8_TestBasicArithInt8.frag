#version 450 core
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

layout(location = 0) out int8_t oColor;

layout(binding = 0) uniform Buf
{
    ivec2 v2;
} buf;

void main()
{
    int8_t a = int8_t(buf.v2.x);
    int8_t b = int8_t(buf.v2.y);
    int8_t c = a + b;
    int8_t d = a - b;
    int8_t e = -a;
    int8_t f = a * b;
    int8_t g = a / b;
    int8_t h = a % b;

    bool bl = a == b;
    bl = bl && (c != d);
    bl = bl && (e < f);
    bl = bl && (g > h);
    bl = bl && (a >= c);
    bl = bl && (a <= d);
    int8_t z = bl ? c + d + e : f + g + h;
    oColor = z;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST

