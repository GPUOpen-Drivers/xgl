#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 c0;
    vec4 c1;
};

layout(location = 0) out vec4 o1;

void main()
{
    o1 = c0 + c1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
