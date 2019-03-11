#version 450

struct str
{
    float f;
};

layout(set = 0, binding = 0, std140) uniform BB
{
   layout(align = 32) vec4 m1;
   layout(align = 64) str m2;
   layout(align = 256) vec2 m3;
};

layout(location = 0) out vec4 o1;
layout(location = 1) out float o2;
layout(location = 2) out vec2 o3;

void main()
{
    o1 = m1;
    o2 = m2.f;
    o3 = m3;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[[0-9]*}}, i32 0, i1 true, i32 0
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[[0-9]*}}, i32 64, i1 true, i32 0
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[[0-9]*}}, i32 256, i1 true, i32 0

; SHADERTEST-LABEL: {{^// LLPC.*}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 64, i32 0)
; SHADERTEST: call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32(<4 x i32> %{{[0-9]*}}, i32 256, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
