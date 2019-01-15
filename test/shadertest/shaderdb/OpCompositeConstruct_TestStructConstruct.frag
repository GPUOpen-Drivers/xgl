#version 450

struct DATA
{
    int   i1;
    vec2  f2;
    dvec3 d3;
};

layout(binding = 0) uniform Uniforms
{
    int   i1;
    vec2  f2;
    dvec3 d3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    DATA data = DATA(i1, f2, d3);

    fragColor = vec4(data.f2, data.f2);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
