#version 450

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform Uniforms
{
    vec3 color;
};

void main()
{
    vec4 data  = vec4(0.5);

    data.xw = color.zy;

    fragColor = data;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = extractelement <2 x float> %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = insertelement <4 x float> undef, float %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = extractelement <2 x float> %{{.*}}, i32 1
; SHADERTEST: %{{.*}} = insertelement <4 x float> %{{.*}}, float %{{.*}}, i32 3,
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{.*}} = extractelement <3 x float> %{{.*}}, i32 2
; SHADERTEST: %{{.*}} = insertelement <4 x float> <float undef, float {{.*}}, float {{.*}}, float undef>, float %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = extractelement <3 x float> %{{.*}}, i32 1
; SHADERTEST: %{{.*}} = insertelement <4 x float> %{{.*}}, float %{{.*}}, i32 3
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
