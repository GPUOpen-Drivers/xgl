#version 450 core

layout(location = 0) in vec4 a;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fv = radians(a);
    fv.x = radians(1.5);
    frag_color = fv;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z7radiansDv4_f(<4 x float> %{{.*}})
; SHADERTEST: store float 0x3F9ACEEA00000000, float addrspace(5)* %{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fmul <4 x float> %{{.*}}, <float undef, float 0x3F91DF46A0000000, float 0x3F91DF46A0000000, float 0x3F91DF46A0000000>
; SHADERTEST: %{{.*}} = insertelement <4 x float> %{{.*}}, float 0x3F9ACEEA00000000, i32 0
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{.*}} = fmul float %{{.*}}, 0x3F91DF46A0000000
; SHADERTEST: %{{.*}} = fmul float %{{.*}}, 0x3F91DF46A0000000
; SHADERTEST: %{{.*}} = fmul float %{{.*}}, 0x3F91DF46A0000000
; SHADERTEST: {{.*}} call void @llvm.amdgcn.exp.f32(i32 0, i32 15, float 0x3F9ACEEA00000000, float %{{.*}}, float %{{.*}}, float %{{.*}}, i1 true, i1 true)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
