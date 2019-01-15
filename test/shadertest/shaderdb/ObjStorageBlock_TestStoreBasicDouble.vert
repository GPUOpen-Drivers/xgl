#version 450 core

layout(std430, binding = 0) buffer Block
{
    double d1;
    dvec2  d2;
    dvec3  d3;
    dvec4  d4;
} block;

void main()
{
    block.d1 += 1.0;
    block.d2 += dvec2(2.0);
    block.d3 += dvec3(3.0);
    block.d4 += dvec4(4.0);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
