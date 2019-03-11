#version 450 core

layout(std430, binding = 0) buffer Block
{
    vec4 f4;
    mat4 m4;
} block;

void main()
{
    vec4 f4 = block.f4;
    mat4 m4 = mat4(f4, f4, f4, f4);
    block.m4 = m4;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 48, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 64, <16 x i8> %{{[0-9]*}}, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
