#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler2DShadow        samp2DShadow[4];
layout(set = 1, binding = 0) uniform sampler2DArrayShadow   samp2DArrayShadow;

layout(set = 2, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 fragColor = vec4(0.0);
    float texel = 0.0;

    sparseTextureGradClampARB(samp2DShadow[index], vec3(0.1), vec2(1.0), vec2(1.1), lodClamp, texel);
    fragColor += texel;

    sparseTextureGradClampARB(samp2DArrayShadow, vec4(0.2), vec2(1.2), vec2(1.3), lodClamp, texel);
    fragColor += texel;

    sparseTextureGradOffsetClampARB(samp2DShadow[index], vec3(0.1), vec2(1.0), vec2(1.1), ivec2(2), lodClamp, texel);
    fragColor += texel;

    sparseTextureGradOffsetClampARB(samp2DArrayShadow, vec4(0.2), vec2(1.2), vec2(1.3), ivec2(3), lodClamp, texel);
    fragColor += texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
