#version 450 core

struct S
{
    int  i;
    vec4 f4;
    mat4 m4;
};

layout(std430, binding = 0) buffer Block
{
    S    s0;
    S    s1;
} block;

void main()
{
    block.s1 = block.s0;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 96, <4 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 112, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 128, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 144, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 160, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 176, <16 x i8> %{{[0-9]*}}, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
