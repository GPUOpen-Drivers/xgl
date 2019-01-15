#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1, f1_2, f1_3;
    vec3 f3_1, f3_2, f3_3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = fma(f1_1, f1_2, f1_3);

    vec3 f3_0 = fma(f3_1, f3_2, f3_3);

    fragColor = (f3_0.x != f1_0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
