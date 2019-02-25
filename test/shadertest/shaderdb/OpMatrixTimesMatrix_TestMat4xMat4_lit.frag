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
  mat4 m1;
  mat4 m2;
};

void main()
{
    mat4 cm = m1*m2;
    color = cm[0] + cm[1] + cm[2] + cm[3];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [4 x <4 x float>] @_Z17MatrixTimesMatrixDv4_Dv4_fDv4_Dv4_f

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <4 x float> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd <4 x float> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
