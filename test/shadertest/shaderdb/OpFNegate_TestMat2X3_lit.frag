#version 450

layout(binding = 0) uniform Uniforms
{
    mat2x3 m2x3_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    mat2x3 m2x3_1 = -m2x3_0;

    fragColor = ((m2x3_1[0][0] + m2x3_1[1][2]) != 0.0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-2: fsub <3 x float>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
