#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 ca[3];
    int i;
};

layout(location = 0) out vec4 o1;

void main()
{
    o1 = ca[i];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
