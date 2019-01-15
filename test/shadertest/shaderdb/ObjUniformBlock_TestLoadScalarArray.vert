#version 450 core

layout(std140, binding = 0) uniform Block
{
    float f1[2];
    int  i;
} block;

void main()
{
    int i = block.i;
    float f1[2] = block.f1;
    gl_Position = vec4(f1[i]);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
