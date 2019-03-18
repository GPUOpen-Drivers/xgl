#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1, f1_2;
    dvec2 d2_1, d2_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = f1_1 + f1_2;

    dvec2 d2_0 = d2_1 + d2_2;

    fragColor = ((f1_0 != 0.0) || (d2_0.x != d2_0.y)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST-COUNT-2: fadd reassoc nnan arcp contract <2 x double>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
