#version 450 core

struct S0
{
    int  i;
    vec4 f4;
};

struct S1
{
    int  i;
    S0   s0;
};

layout(std140, binding = 0) uniform Block
{
    S1 s1;
} block;

void main()
{
    S1 s1 = block.s1;
    gl_Position = (s1.i > s1.s0.i) ? s1.s0.f4 : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
