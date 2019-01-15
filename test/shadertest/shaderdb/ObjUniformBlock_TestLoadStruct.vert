#version 450 core

struct S
{
    int  i;
    vec4 f4;
    mat4 m4;
};

layout(std140, binding = 0) uniform Block
{
    S s;
} block;

void main()
{
    S s = block.s;
    gl_Position = (s.i > 0) ? s.f4 : s.m4[s.i];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
