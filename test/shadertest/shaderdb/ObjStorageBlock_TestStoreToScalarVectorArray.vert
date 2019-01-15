#version 450 core

layout(std430, binding = 0) buffer Block
{
    int  i1[12];
    vec4 f4[16];
    int  i;
} block;

void main()
{
    int i = block.i;
    block.i1[7] = 23;
    block.i1[i] = 45;
    block.f4[3] = vec4(2.0);
    block.f4[i] = vec4(3.0);

    gl_Position = vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
