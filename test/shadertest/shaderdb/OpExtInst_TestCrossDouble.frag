#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3_1;
    dvec3 d3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3_0 = cross(d3_1, d3_2);

    fragColor = (d3_0 != d3_1) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
