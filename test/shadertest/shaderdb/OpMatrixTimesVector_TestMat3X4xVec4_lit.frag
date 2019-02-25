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
    color = m2 * colorIn1.xyz;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @_Z17MatrixTimesVectorDv3_Dv4_fDv3_f

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <4 x float>
; SHADERTEST: fadd <4 x float>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
