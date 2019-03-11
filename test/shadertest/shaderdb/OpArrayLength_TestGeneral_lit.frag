#version 450 core

layout(std430, set = 0, binding = 0) buffer Data
{
    int  i1;
    vec3 f3;
    bool b1[];
} data[4];

layout(std430, set = 0, binding = 1) buffer OtherData
{
    vec2 f2;
    mat4 m4[];
} otherData;

layout(location = 0) out vec4 f;

void main()
{
    int len = data[2].b1.length();
    len += otherData.m4.length();
    f = vec4(len);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.buffer.arraylength(<4 x i32> %{{[0-9]*}}, i32 28, i32 4)
; SHADERTEST: call i32 @llpc.buffer.arraylength(<4 x i32> %{{[0-9]*}}, i32 16, i32 64)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
