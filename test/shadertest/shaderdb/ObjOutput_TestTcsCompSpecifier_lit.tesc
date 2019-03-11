#version 450 core

layout(vertices = 3) out;

layout(location = 0) out vec2 f2[];
layout(location = 0, component = 2) out float f1[];

void main(void)
{
    vec4 f4 = gl_in[gl_InvocationID].gl_Position;
    f2[gl_InvocationID] = f4.xy;
    f1[gl_InvocationID] = f4.w;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32(i32 0, i32 0, i32 0, i32 %{{[0-9]*}}, <2 x float> %{{[0-9]*}})
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32(i32 0, i32 0, i32 2, i32 %{{[0-9]*}}, float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
