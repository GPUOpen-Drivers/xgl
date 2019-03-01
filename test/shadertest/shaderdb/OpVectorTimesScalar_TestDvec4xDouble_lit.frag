#version 450

layout(binding = 0) uniform Uniforms
{
    dvec4 d3_1;
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec4 d3_0 = dvec4(0.0);
    d3_0 *= d1;

    fragColor = (d3_0 != d3_1) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = insertelement <4 x double> undef, double %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = shufflevector <4 x double> %{{.*}}, <4 x double> undef, <4 x i32> zeroinitializer
; SHADERTEST: %{{.*}} = fmul <4 x double> %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
