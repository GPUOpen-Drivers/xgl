#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3_0, d3_1;
    dvec4 d4_0, d4_1;
    dvec2 d2_0, d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    double d1 = dot(d3_0, d3_1);
    d1 += dot(d4_0, d4_1);
    d1 += dot(d2_0, d2_1);

    fragColor = (d1 > 0.0) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} double @_Z3dotDv3_dDv3_d(<3 x double> %{{[0-9]+}}, <3 x double> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} double @_Z3dotDv4_dDv4_d(<4 x double> %{{[0-9]+}}, <4 x double> %{{[0-9]+}})
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} double @_Z3dotDv2_dDv2_d(<2 x double> %{{[0-9]+}}, <2 x double> %{{[0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
