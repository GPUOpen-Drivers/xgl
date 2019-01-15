#version 450 core
#extension GL_AMD_gpu_shader_int16 : enable

layout(location = 0) out int16_t oColor;

void main()
{
    int16_t a = 1s;
    int16_t b = 2s;
    int16_t c = a + b;
    int16_t d = a - b;
    int16_t e = -a;
    int16_t f = a * b;
    int16_t g = a / b;
    int16_t h = a % b;
    bool bl = a == b;
    bl = bl && (a != b);
    bl = bl && (a < b);
    bl = bl && (a > b);
    bl = bl && (a >= b);
    bl = bl && (a <= b);
    int16_t z = bl ? c + d + e : f + g + h;
    oColor = z;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
