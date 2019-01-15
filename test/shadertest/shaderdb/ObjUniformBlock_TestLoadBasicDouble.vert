#version 450 core

layout(std140, binding = 0) uniform Block
{
    double d1;
    dvec2  d2;
    dvec3  d3;
    dvec4  d4;
} block;

void main()
{
    dvec4 d4 = block.d4;
    d4.xyz += block.d3;
    d4.xy  += block.d2;
    d4.x   += block.d1;

    gl_Position = vec4(d4);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
