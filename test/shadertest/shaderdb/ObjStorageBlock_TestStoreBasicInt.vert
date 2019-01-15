#version 450 core

layout(std430, binding = 0) buffer Block
{
    int   i1;
    ivec2 i2;
    ivec3 i3;
    ivec4 i4;
} block;

void main()
{
    block.i1 += 1;
    block.i2 += ivec2(2);
    block.i3 += ivec3(3);
    block.i4 += ivec4(4);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
