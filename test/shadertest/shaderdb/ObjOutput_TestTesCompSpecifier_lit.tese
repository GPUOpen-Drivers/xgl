#version 450 core

layout(triangles) in;

layout(location = 0) out vec2 f2;
layout(location = 0, component = 2) out float f1;

void main(void)
{
    vec3 f3 = gl_in[1].gl_Position.xyz;
    f2 = f3.yz;
    f1 = f3.x;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32(i32 0, i32 0, <2 x float> %{{[0-9]*}})
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32(i32 0, i32 2, float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
