#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec4 d4_0 = dvec4(0), d4_1 = dvec4(0);

    d4_0.wz = d3.yx;
    d4_1.xw = d3.zz;

    if (d4_0 != d4_1)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = extractelement <3 x double> %{{.*}}, i32 1
; SHADERTEST: %{{.*}} = insertelement <2 x double> undef, double %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = extractelement <3 x double> %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = insertelement <2 x double> %{{.*}}, double %{{.*}}, i32 1
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{.*}} = shufflevector <3 x double> %{{.*}}, <3 x double> undef, <4 x i32> <i32 0, i32 1, i32 undef, i32 undef>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
