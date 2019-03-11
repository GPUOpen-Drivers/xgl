#version 450 core

layout(vertices = 3) out;

layout(location = 0) in vec2 f2[];
layout(location = 0, component = 2) in float f1[];

void main(void)
{
    vec3 f3 = vec3(f1[gl_InvocationID], f2[gl_InvocationID]);
    gl_out[gl_InvocationID].gl_Position = vec4(f3, 1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call float @llpc.input.import.generic.f32{{.*}}(i32 0, i32 0, i32 2, i32 %{{[0-9]*}})
; SHADERTEST: call <2 x float> @llpc.input.import.generic.v2f32{{.*}}(i32 0, i32 0, i32 0, i32 %{{[0-9]*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
