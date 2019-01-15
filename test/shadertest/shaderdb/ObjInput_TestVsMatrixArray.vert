#version 450 core

layout(location = 2) in mat4 m4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    gl_Position = m4[i][1];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
