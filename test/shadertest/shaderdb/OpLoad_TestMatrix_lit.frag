#version 450

layout(set  = 0, binding = 0) uniform Uniforms
{
    mat3 m;
    int index;
};

layout(set = 1, binding = 1) uniform BB
{
    mat3 m2;
    layout (row_major) mat2x3 m3;
};

layout(location = 0) out vec3 o1;
layout(location = 1) out vec3 o2;
layout(location = 2) out vec3 o3;

void main()
{
    o1 = m[2];
    o2 = m2[2] + m3[1];
    o3 = m2[index] + m3[index];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: load <3 x float>, <3 x float>

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <12 x i8> @llpc.buffer.load.v12i8
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <2 x i32> @llvm.amdgcn.s.buffer.load.v2i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
