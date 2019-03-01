#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;

    vec3 f3_1, f3_2;
    vec4 f4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3_0 = step(f3_1, f3_2);

    vec4 f4_0 = step(f1_1, f4_1);

    fragColor = (f3_0.y == f4_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x float> @_Z4stepDv3_fDv3_f(<3 x float> %{{.*}}, <3 x float> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z4stepDv4_fDv4_f(<4 x float> %{{.*}}, <4 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fcmp olt float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, float 0.000000e+00, float 1.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp olt float %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, float 0.000000e+00, float 1.000000e+00
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
