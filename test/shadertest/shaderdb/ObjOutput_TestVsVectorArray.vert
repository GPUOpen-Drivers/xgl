#version 450 core

layout(location = 0) in float f1;
layout(location = 3) out vec4 f4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    f4[i] = vec4(f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
