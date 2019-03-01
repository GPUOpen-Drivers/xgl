#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = tanh(f1_1);

    vec3 f3_0 = tanh(f3_1);

    fragColor = (f1_0 != f3_0.x) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z4tanhf(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z4tanhDv3_f(<3 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = fmul float %{{.*}}, 0x3FF7154760000000
; SHADERTEST: %{{[0-9]*}} = fsub float 0.000000e+00, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.exp2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.exp2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = fadd float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.fdiv.fast(float %{{.*}}, float %{{.*}})
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
