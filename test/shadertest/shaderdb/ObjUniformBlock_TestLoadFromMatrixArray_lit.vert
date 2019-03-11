#version 450 core

layout(std140, binding = 0) uniform Block
{
    mat4 m4[4];
    int  i;
} block;

void main()
{
    int i = block.i;
    gl_Position = block.m4[2][i] + block.m4[3][3];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 240, i1 true, i32 0, i1 false)

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 256, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 240, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
