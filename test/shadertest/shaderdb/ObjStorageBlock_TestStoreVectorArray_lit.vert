#version 450 core

layout(std430, binding = 0) buffer Block
{
    float f1;
    vec4  f4[2];
} block;

void main()
{
    float f1 = block.f1;
    vec4 f4[2];
    f4[0] = vec4(f1);
    f4[1] = vec4(1.0);
    block.f4 = f4;

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16, <16 x i8> %{{[0-9]*}}, i32 0
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32, <16 x i8> <i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63>, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 16, i32 0, i32 0)
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> <float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00>, <4 x i32> %{{[0-9]*}}, i32 32, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
