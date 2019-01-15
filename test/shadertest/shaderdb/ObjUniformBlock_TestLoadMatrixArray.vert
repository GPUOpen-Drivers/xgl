#version 450 core

layout(std140, binding = 0) uniform Block
{
    int  i;
    mat4 m4[2];
} block;

void main()
{
    int i = block.i;
    mat4 m4[2] = block.m4;
    gl_Position = m4[i][i];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
