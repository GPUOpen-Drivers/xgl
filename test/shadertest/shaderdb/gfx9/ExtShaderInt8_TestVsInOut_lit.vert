#version 450 core

#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable

layout(location = 0) in int8_t i8In;
layout(location = 1) in u8vec3 u8v3In;

layout(location = 0) out int8_t i8Out;
layout(location = 1) out u8vec3 u8v3Out;

void main (void)
{
    i8Out   = i8In;
    u8v3Out = u8v3In;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic.i32.i32.i8(i32 0, i32 0, i8 %{{[0-9]*}})
; SHADERTEST: call void @llpc.output.export.generic.i32.i32.v3i8(i32 1, i32 0, <3 x i8> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32(i32 32, i32 1, float %{{[0-9]*}}, float undef, float undef, float undef, i1 false, i1 false)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
