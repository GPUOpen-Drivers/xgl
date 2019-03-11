#version 450 core

layout(std140, binding = 0) uniform Block
{
    float f1;
    vec2  f2;
    vec3  f3;
    vec4  f4;
} block;

void main()
{
    vec4 f4 = block.f4;
    f4.xyz += block.f3;
    f4.xy  += block.f2;
    f4.x   += block.f1;

    gl_Position = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[[0-9]*}}, i32 32, i1 true, i32 0, i1 false)
; SHADERTEST: call <12 x i8> @llpc.buffer.load.v12i8(<4 x i32> %{{[[0-9]*}}, i32 16, i1 true, i32 0, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[[0-9]*}}, i32 8, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[[0-9]*}}, i32 0, i1 true, i32 0, i1 false)

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 32, i32 0)
; SHADERTEST: call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %{{[0-9]*}}, i32 16, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 24, i32 0)
; SHADERTEST: call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %{{[0-9]*}}, i32 8, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
