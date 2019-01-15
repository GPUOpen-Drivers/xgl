#version 450

layout(location = 0) out vec4 fragColor;

vec4 func(inout int);

void main()
{
    int i1 = 2;
    fragColor = func(i1);
}

vec4 func(inout int i1)
{
    vec4 v4_0, v4_1;
    i1 = 1;

    return (i1 != 0) ? v4_0 : v4_1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
