#version 450 core

layout(location = 0) in vec4 a;
layout(location = 0) out vec4 frag_color;

void main()
{
    float fv = tanh(a.x);
    frag_color = vec4(fv);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z4tanhf(float %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 0x3FF7154760000000
; SHADERTEST: %{{[0-9]*}} = fsub float 0.000000e+00, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.exp2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.exp2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fadd float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.fdiv.fast(float %{{.*}}, float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
