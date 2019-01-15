#version 450

layout(binding = 0) uniform Uniforms
{
    float f1;
    double d1;
    dvec3  d3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4  f4 = vec4(f1, f1, 0.5, 1.0);

    dvec3  d3_0 = dvec3(d1);

    fragColor = (d3_0 == d3_1) ? f4 : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
