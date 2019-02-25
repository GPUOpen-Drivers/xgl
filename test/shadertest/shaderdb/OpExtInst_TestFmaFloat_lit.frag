#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1, f1_2, f1_3;
    vec3 f3_1, f3_2, f3_3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = fma(f1_1, f1_2, f1_3);

    vec3 f3_0 = fma(f3_1, f3_2, f3_3);

    fragColor = (f3_0.x != f1_0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z3fmafff(float %{{.*}}, float %{{.*}}, float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z3fmaDv3_fDv3_fDv3_f(<3 x float> %{{.*}}, <3 x float> %{{.*}}, <3 x float> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
