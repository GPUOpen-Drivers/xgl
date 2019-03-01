#version 450 core

layout(location = 0) in vec4 a;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fv = tan(a);
    fv.x = tan(1.5);
    frag_color = fv;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z3tanDv4_f(<4 x float> %0)
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @_Z4fdivff(float 1.000000e+00, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @_Z4fdivff(float 1.000000e+00, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @_Z4fdivff(float 1.000000e+00, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fdiv float 1.000000e+00, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fdiv float 1.000000e+00, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.sin.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.cos.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fdiv float 1.000000e+00, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
