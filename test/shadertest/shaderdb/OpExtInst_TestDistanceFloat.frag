#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    float f1_2;

    vec3 f3_0;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = distance(f1_1, f1_2);

    f1_0 += distance(f3_0, f3_1);

    fragColor = (f1_0 > 0.0) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
