#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0, f1_2;
    f1_0 = modf(f1_1, f1_2);

    vec3 f3_0, f3_2;
    f3_0 = modf(f3_1, f3_2);

    fragColor = ((f1_0 != f3_0.x) || (f1_2 == f3_2.y)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z4modffPf(float %{{.*}}, float addrspace(5)* %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z4modfDv3_fPDv3_f(<3 x float> %{{.*}}, <3 x float> addrspace(5)* %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = call float @llvm.trunc.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = fsub float %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
