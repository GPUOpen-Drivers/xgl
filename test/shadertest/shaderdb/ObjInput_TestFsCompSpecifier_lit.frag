#version 450 core

layout(location = 0, component = 1) in vec2 f2;
layout(location = 0, component = 3) in float f1;

layout(location = 0) out vec3 f3;

void main (void)
{
    f3 = vec3(f1, f2);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <2 x float> @llpc.input.import.generic.v2f32{{.*}}
; SHADERTEST: call float @llpc.input.import.generic.f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call float @llvm.amdgcn.interp.p1(float %{{[0-9]*}}, i32 1, i32 0, i32 %{{[0-9]*}})
; SHADERTEST: call float @llvm.amdgcn.interp.p2(float %{{[0-9]*}}, float %{{[0-9]*}}, i32 1, i32 0, i32 %{{[0-9]*}})
; SHADERTEST: call float @llvm.amdgcn.interp.p1(float %{{[0-9]*}}, i32 3, i32 0, i32 %{{[0-9]*}})
; SHADERTEST: call float @llvm.amdgcn.interp.p2(float %{{[0-9]*}}, float %{{[0-9]*}}, i32 3, i32 0, i32 %{{[0-9]*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
