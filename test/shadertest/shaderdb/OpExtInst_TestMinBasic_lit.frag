#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fmin = min(a,b);
    ivec4 imin = min(ivec4(a), ivec4(b));
    uvec4 umin = min(uvec4(a), uvec4(b));
    frag_color = fmin + vec4(imin) + vec4(umin);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z4fminDv4_fDv4_f(<4 x float> %{{.*}}, <4 x float> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x i32> @_Z4sminDv4_iDv4_i(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x i32> @_Z4uminDv4_iDv4_i(<4 x i32> %{{.*}}, <4 x i32> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.minnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.minnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.minnum.f32(float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.minnum.f32(float %{{.*}}, float %{{.*}})
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
