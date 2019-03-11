#version 450 core

layout(std140, binding = 0, row_major) uniform Block
{
    vec3   f;
    mat2x3 m2x3;
} block;

layout(location = 0) out vec3 f;

void main()
{
    mat2x3 m2x3 = block.m2x3;
    f = m2x3[1] + block.f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 16, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 32, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 48, i1 true, i32 0, i1 false)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
