#version 450 core

layout(std140, binding = 0) uniform Block
{
    int   i1;
    ivec2 i2;
    ivec3 i3;
    ivec4 i4;
} block;

void main()
{
    ivec4 i4 = block.i4;
    i4.xyz += block.i3;
    i4.xy  += block.i2;
    i4.x   += block.i1;

    gl_Position = vec4(i4);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
