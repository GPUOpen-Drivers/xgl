#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec2 colorIn2;
layout(location = 2) in vec2 colorIn3;

layout(location = 0) out vec4 color;

struct AA
{
   mat4 bb;
};

layout(binding=2) uniform BB
{
  mat3x4 m2;
};

void main()
{
    mat3x4 m3 = m2 * colorIn1.x;
    color = m3[0] + m3[1] + m3[2];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [3 x <4 x float>] @_Z17MatrixTimesScalarDv3_Dv4_ff

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul float

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
