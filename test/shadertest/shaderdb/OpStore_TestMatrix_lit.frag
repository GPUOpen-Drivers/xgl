#version 450

layout(binding = 0) uniform Uniforms
{
    mat3 m;
};

mat3 m2;
layout(location = 1) out vec3 o1;

void main()
{
    m2 = m;
    o1 = m2[2];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: store <3 x float>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
