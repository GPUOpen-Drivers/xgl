#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    double d1_2;

    dvec3 d3_0;
    dvec3 d3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = distance(d1_1, d1_2);

    d1_0 += distance(d3_0, d3_1);

    fragColor = (d1_0 != d1_1) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
