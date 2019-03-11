#version 450 core

layout(std430, binding = 0) buffer Block
{
    vec4 f4;
    mat4 m4[2];
} block;

void main()
{
    vec4 f4 = block.f4;
    mat4 m4[2];
    m4[0] = mat4(vec4(1.0), vec4(1.0), f4, f4);
    m4[1] = mat4(f4, f4, vec4(0.0), vec4(0.0));
    block.m4 = m4;

    gl_Position = vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16, <16 x i8> <i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32, <16 x i8> <i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 48, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 64, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 80, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 96, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 112, <16 x i8> zeroinitializer, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 128, <16 x i8> zeroinitializer, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> <float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00>, <4 x i32> %{{[0-9]*}}, i32 16, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> <float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00>, <4 x i32> %{{[0-9]*}}, i32 32, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 48, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 64, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 80, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 96, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> zeroinitializer, <4 x i32> %{{[0-9]*}}, i32 112, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> zeroinitializer, <4 x i32> %{{[0-9]*}}, i32 128, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
