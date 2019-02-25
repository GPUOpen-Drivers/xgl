#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1, d1_2, d1_3;
    dvec3 d3_1, d3_2, d3_3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = fma(d1_1, d1_2, d1_3);

    dvec3 d3_0 = fma(d3_1, d3_2, d3_3);

    fragColor = (d3_0.x != d1_0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z3fmaddd(double %{{.*}}, double %{{.*}}, double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x double> @_Z3fmaDv3_dDv3_dDv3_d(<3 x double> %{{.*}}, <3 x double> %{{.*}}, <3 x double> %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
