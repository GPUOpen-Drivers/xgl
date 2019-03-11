#version 450 core

layout(location = 0, component = 0) in float f1;
layout(location = 0, component = 1) in vec2  f2;

layout(location = 0) out vec3 color;

void main()
{
    vec3 f3 = vec3(f1, f2);
    color = f3;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <2 x float> @llpc.input.import.generic.v2f32.i32.i32(i32 0, i32 1)
; SHADERTEST: call float @llpc.input.import.generic.f32.i32.i32(i32 0, i32 0)
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.struct.tbuffer.load.v4i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
