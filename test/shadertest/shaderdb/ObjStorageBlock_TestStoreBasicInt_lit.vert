#version 450 core

layout(std430, binding = 0) buffer Block
{
    int   i1;
    ivec2 i2;
    ivec3 i3;
    ivec4 i4;
} block;

void main()
{
    block.i1 += 1;
    block.i2 += ivec2(2);
    block.i3 += ivec3(3);
    block.i4 += ivec4(4);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call <12 x i8> @llpc.buffer.load.v12i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call void @llpc.buffer.store.v12i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
