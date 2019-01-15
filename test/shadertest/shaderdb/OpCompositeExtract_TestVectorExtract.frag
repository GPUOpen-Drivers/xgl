#version 450

layout(binding = 0) uniform Uniforms
{
    ivec4 i4;
    dvec3 d3;
    double d1_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1 = i4.w;

    double d1_0 = d3.z;

    fragColor = ((i1 == 1) && (d1_0 != d1_1)) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
