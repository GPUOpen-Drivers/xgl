#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;

    dvec3 d3_1, d3_2;
    dvec4 d4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3_0 = step(d3_1, d3_2);

    dvec4 d4_0 = step(d1_1, d4_1);

    fragColor = (d3_0.y == d4_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <3 x double> @_Z4stepDv3_dDv3_d(<3 x double> %{{.*}}, <3 x double> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x double> @_Z4stepDv4_dDv4_d(<4 x double> %{{.*}}, <4 x double> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fcmp olt double %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, double 0.000000e+00, double 1.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp olt double %{{.*}}, %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, double 0.000000e+00, double 1.000000e+00
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
