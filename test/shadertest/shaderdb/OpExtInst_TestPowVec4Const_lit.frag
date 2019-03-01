#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fv = pow(a, b);
    fv.x = pow(2.0, 3.0) + pow(-2.0, 2.0);
    frag_color = fv;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z3powDv4_fDv4_f(<4 x float> %{{.*}}, <4 x float> %{{.*}})
; SHADERTEST: store float 1.200000e+01, float addrspace(5)* %{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = insertelement <4 x float> <float 1.200000e+01, float undef, float undef, float undef>
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.pow.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: {{.*}} call void @llvm.amdgcn.exp.f32(i32 0, i32 15, float 1.200000e+01, float %{{.*}}, float %{{.*}}, float %{{.*}}, i1 true, i1 true)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
