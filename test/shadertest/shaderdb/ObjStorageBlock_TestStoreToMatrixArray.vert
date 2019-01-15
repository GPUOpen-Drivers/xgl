#version 450 core

layout(std430, binding = 0) buffer Block
{
    int  i;
    mat4 m4[16];
} block;

void main()
{
    int i = block.i;
    block.m4[3][3] = vec4(2.0);
    block.m4[i][i] = vec4(3.0);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
