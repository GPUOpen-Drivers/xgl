#version 450 core

layout(std430, binding = 0) buffer Block
{
    double d1;
    dvec2  d2;
    dvec3  d3;
    dvec4  d4;
} block;

void main()
{
    block.d1 += 1.0;
    block.d2 += dvec2(2.0);
    block.d3 += dvec3(3.0);
    block.d4 += dvec4(4.0);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16
; SHADERTEST: call <24 x i8> @llpc.buffer.load.v24i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call void @llpc.buffer.store.v24i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call <32 x i8> @llpc.buffer.load.v32i8(<4 x i32> %{{[0-9]*}}, i32 64
; SHADERTEST: call void @llpc.buffer.store.v32i8(<4 x i32> %{{[0-9]*}}, i32 64

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
