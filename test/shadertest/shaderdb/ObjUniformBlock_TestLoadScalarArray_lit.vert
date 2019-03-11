#version 450 core

layout(std140, binding = 0) uniform Block
{
    float f1[2];
    int  i;
} block;

void main()
{
    int i = block.i;
    float f1[2] = block.f1;
    f1[i] = 2.0;
    gl_Position = vec4(f1[i]);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 0, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 16, i1 true, i32 0, i1 false)
; SHADERTEST: store float 2.000000e+00, float addrspace({{.*}})* %{{[0-9]*}}

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 32, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 16, i32 0)
; SHADERTEST: store float 2.000000e+00, float addrspace({{.*}})* %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
