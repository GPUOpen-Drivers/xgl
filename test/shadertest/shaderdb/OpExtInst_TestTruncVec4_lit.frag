#version 450 core

layout(location = 0) in float a;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 b = trunc(b0);
    frag_color = b;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z5truncDv4_f(<4 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
