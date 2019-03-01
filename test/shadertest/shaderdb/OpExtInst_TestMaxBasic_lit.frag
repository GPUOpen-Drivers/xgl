#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fmax = max(a,b);
    ivec4 imax = max(ivec4(a), ivec4(b));
    uvec4 umax = max(uvec4(a), uvec4(b));
    frag_color = fmax + vec4(imax) + vec4(umax);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z4fmaxDv4_fDv4_f(<4 x float> %{{.*}}, <4 x float> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x i32> @_Z4smaxDv4_iDv4_i(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x i32> @_Z4umaxDv4_iDv4_i(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.maxnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.maxnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.maxnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.maxnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp slt i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp ult i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp ult i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp ult i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = icmp ult i32 %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
