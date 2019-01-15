#version 450 core

layout(std140, binding = 0) uniform Block
{
    float f1;
    vec2  f2;
    vec3  f3;
    vec4  f4;
} block;

void main()
{
    vec4 f4 = block.f4;
    f4.xyz += block.f3;
    f4.xy  += block.f2;
    f4.x   += block.f1;

    gl_Position = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
