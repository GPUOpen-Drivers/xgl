#version 450 core

struct S
{
    int  i1;
    vec3 f3;
    mat4 m4;
};

layout(location = 4) out S s;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    s.i1 = 2;
    s.f3 = vec3(1.0);
    s.m4[i] = vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
