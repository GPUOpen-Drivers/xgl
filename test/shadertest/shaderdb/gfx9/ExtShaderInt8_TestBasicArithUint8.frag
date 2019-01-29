#version 450 core
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

layout(location = 0) out uint8_t oColor;

layout(binding = 0) uniform Buf
{
    uvec2 v2;
} buf;

void main()
{
    uint8_t a = uint8_t(buf.v2.x);
    uint8_t b = uint8_t(buf.v2.y);
    uint8_t c = a + b;
    uint8_t d = a - b;
    uint8_t e = -a;
    uint8_t f = a * b;
    uint8_t g = a / b;
    uint8_t h = a % b;

    bool bl = a == b;
    bl = bl && (c != d);
    bl = bl && (e < f);
    bl = bl && (g > h);
    bl = bl && (a >= c);
    bl = bl && (a <= d);
    uint8_t z = bl ? c + d + e : f + g + h;
    oColor = z;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST


