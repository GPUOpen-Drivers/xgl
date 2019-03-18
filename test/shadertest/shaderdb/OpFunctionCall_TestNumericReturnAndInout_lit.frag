#version 450

layout(location = 0) out vec4 fragColor;

vec4 func(inout int);

void main()
{
    int i1 = 2;
    vec4 ret = func(i1);
    ret.w = i1;
    fragColor = ret;
}

vec4 func(inout int i1)
{
    i1 = 1;

    return (i1 != 0) ? vec4(1.0) : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} <4 x float> @"func
; SHADERTEST: define internal {{.*}} <4 x float> @"func({{.*}}"(i32 {{.*}} %i1)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
