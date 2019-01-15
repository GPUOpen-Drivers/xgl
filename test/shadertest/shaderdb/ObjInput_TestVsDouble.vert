#version 450 core

layout(location = 2) in dvec4 d4;
layout(location = 4) in dvec3 d3[2];
layout(location = 8) in dmat4 dm4;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    dvec4 d = d4;
    d += dvec4(d3[i], 1.0);
    d += dm4[i];

    gl_Position = vec4(d);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
