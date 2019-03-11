#version 450 core

layout(std430, binding = 0) buffer Block
{
    float f1;
    vec2  f2;
    vec3  f3;
    vec4  f4;
} block;

void main()
{
    block.f1 += 1.0;
    block.f2 += vec2(2.0);
    block.f3 += vec3(3.0);
    block.f4 += vec4(4.0);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 8,
; SHADERTEST: call <12 x i8> @llpc.buffer.load.v12i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call void @llpc.buffer.store.v12i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
