#version 450 core
#extension GL_AMD_gpu_shader_int16 : enable

layout(location = 0) out uint16_t oColor;

void main()
{
    uint16_t a = 1us;
    uint16_t b = 2us;
    uint16_t c = a + b;
    uint16_t d = a - b;
    uint16_t f = a * b;
    uint16_t g = a / b;
    uint16_t h = a % b;
    bool bl = a == b;
    bl = bl && (a != b);
    bl = bl && (a < b);
    bl = bl && (a > b);
    bl = bl && (a >= b);
    bl = bl && (a <= b);
    uint16_t z = bl ? c + d : f + g + h;
    oColor = z;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
