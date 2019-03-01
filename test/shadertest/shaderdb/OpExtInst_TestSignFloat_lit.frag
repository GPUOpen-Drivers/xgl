#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = sign(f1_1);

    vec3 f3_0 = sign(f3_1);

    fragColor = ((f1_0 != f3_0.x)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z5fsignf(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z5fsignDv3_f(<3 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
