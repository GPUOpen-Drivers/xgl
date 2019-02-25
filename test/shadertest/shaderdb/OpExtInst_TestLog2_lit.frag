#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = log2(f1_1);

    vec3 f3_0 = log2(f3_1);

    fragColor = ((f1_0 != 0.0) && (f3_0.x < 0.1)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z4log2f(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z4log2Dv3_f(<3 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.log2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.log2.f32(float %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.log2.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.log2.f32(float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
