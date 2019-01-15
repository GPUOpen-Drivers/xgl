#version 450 core

layout(std430, binding = 0) buffer Block
{
    vec4 f4;
    mat4 m4;
} block;

void main()
{
    vec4 f4 = block.f4;
    mat4 m4 = mat4(f4, f4, f4, f4);
    block.m4 = m4;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
