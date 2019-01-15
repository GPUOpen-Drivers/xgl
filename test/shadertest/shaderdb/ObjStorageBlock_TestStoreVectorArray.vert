#version 450 core

layout(std430, binding = 0) buffer Block
{
    float f1;
    vec4  f4[2];
} block;

void main()
{
    float f1 = block.f1;
    vec4 f4[2];
    f4[0] = vec4(f1);
    f4[1] = vec4(1.0);
    block.f4 = f4;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
