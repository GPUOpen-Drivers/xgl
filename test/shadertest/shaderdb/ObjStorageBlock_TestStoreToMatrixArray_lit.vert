#version 450 core

layout(std430, binding = 0) buffer Block
{
    int  i;
    mat4 m4[16];
} block;

void main()
{
    int i = block.i;
    block.m4[3][3] = vec4(2.0);
    block.m4[i][i] = vec4(3.0);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 256, <16 x i8> <i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64, i8 0, i8 0, i8 0, i8 64>, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, <16 x i8> <i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64, i8 0, i8 0, i8 64, i8 64>, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> <float 2.000000e+00, float 2.000000e+00, float 2.000000e+00, float 2.000000e+00>, <4 x i32> %{{[0-9]*}}, i32 256, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> <float 3.000000e+00, float 3.000000e+00, float 3.000000e+00, float 3.000000e+00>, <4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
