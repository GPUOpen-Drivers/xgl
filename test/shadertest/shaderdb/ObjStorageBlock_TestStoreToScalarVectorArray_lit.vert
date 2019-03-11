#version 450 core

layout(std430, binding = 0) buffer Block
{
    int  i1[12];
    vec4 f4[16];
    int  i;
} block;

void main()
{
    int i = block.i;
    block.i1[7] = 23;
    block.i1[i] = 45;
    block.f4[3] = vec4(2.0);
    block.f4[i] = vec4(3.0);

    gl_Position = vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 28, <4 x i8> <i8 23, i8 0, i8 0, i8 0>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, <4 x i8> <i8 45, i8 0, i8 0, i8 0>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 96, <16 x i8> <i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, <16 x i8> <i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64>, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
