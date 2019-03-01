#version 450 core


layout(location = 0) in vec4 a0;
layout(location = 10) in vec4 b0;
layout(location = 0) out vec4 color;

void main()
{
    vec4 x = vec4(0);
    vec4 y = modf(a0, x);
    color = vec4(x + y);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z4modfDv4_fPDv4_f(<4 x float> %{{.*}}, <4 x float> addrspace(5)* %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
