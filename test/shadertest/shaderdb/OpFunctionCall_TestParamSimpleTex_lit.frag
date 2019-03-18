#version 450

layout(set = 0, binding = 0) uniform sampler2D samp2D_0;
layout(set = 0, binding = 1) uniform sampler2D samp2D_1;

layout(location = 0) out vec4 fragColor;

vec4 func(sampler2D s2D, vec2 coord)
{
    return texture(s2D, coord);
}

void main()
{
    fragColor  = func(samp2D_0, vec2(1.0));
    fragColor += func(samp2D_1, vec2(0.0));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-2: call {{.*}} <4 x float> @"func
; SHADERTEST: define internal {{.*}} <4 x float> @"func({{.*}};{{.*}};"(%spirv.SampledImage.float{{.*}} %s2D, <2 x float> {{.*}} %coord)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
