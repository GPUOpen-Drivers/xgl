#version 450 core

layout(std140, binding = 0) uniform Block
{
    int  i1[12];
    vec4 f4[16];
    int  i;
} block;

void main()
{
    int i = block.i;
    i = block.i1[7] + block.i1[i];
    gl_Position = block.f4[3] + block.f4[i];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
