#version 450 core

layout(std430, binding = 0, row_major) buffer Block
{
    vec3   f;
    mat2x3 m2x3;
} block;

void main()
{
    mat2x3 m2x3;
    m2x3[0] = vec3(0.1, 0.2, 0.3);
    m2x3[1] = block.f;

    block.m2x3 = m2x3;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 16, <8 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 24, <8 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 32, <8 x i8> %{{[0-9]*}}, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v2f32(<2 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 16, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v2f32(<2 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 24, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v2f32(<2 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 32, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
