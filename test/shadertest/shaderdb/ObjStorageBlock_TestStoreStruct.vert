#version 450 core

struct S
{
    int  i;
    vec4 f4;
    mat4 m4;
};

layout(std430, binding = 0) buffer Block
{
    S    s0;
    S    s1;
} block;

void main()
{
    block.s1 = block.s0;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
