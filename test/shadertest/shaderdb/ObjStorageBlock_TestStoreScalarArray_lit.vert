#version 450 core

layout(std430, binding = 0) buffer Block
{
    int   i;
    float f1[2];
} block;

void main()
{
    int i = block.i;
    float f1[2];
    f1[0] = float(i);
    f1[1] = 2.0;
    block.f1 = f1;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 4, <4 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 8, <4 x i8> <i8 0, i8 0, i8 0, i8 64>, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.f32(float %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 4, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.f32(float 2.000000e+00, <4 x i32> %{{[0-9]*}}, i32 8, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
