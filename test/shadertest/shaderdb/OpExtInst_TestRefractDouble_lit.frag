#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1;
    double d1_2;
    double d1_3;

    dvec4 d4_1;
    dvec4 d4_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = refract(d1_1, d1_2, d1_3);

    dvec4 d4_0 = refract(d4_1, d4_2, d1_3);

    fragColor = (d4_0.x > d1_0) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} double @_Z7refractddd(double %{{.*}}, double %{{.*}}, double %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x double> @_Z7refractDv4_dDv4_dd(<4 x double> %{{.*}}, <4 x double> %{{.*}}, double %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
