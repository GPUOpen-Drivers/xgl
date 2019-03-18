#version 450

layout(binding = 0) uniform Uniforms
{
    mat3 m3_1, m3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    mat3 m3_0 = matrixCompMult(m3_1, m3_2);

    fragColor = (m3_0[0] != (m3_0[1] + m3_0[2])) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST-COUNT-3: fmul reassoc nnan arcp contract <3 x float>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
