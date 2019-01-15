#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler2DShadow            samp2DShadow[4];
layout(set = 0, binding = 0) uniform sampler2DArrayShadow       samp2DArrayShadow;
layout(set = 2, binding = 0) uniform samplerCubeShadow          sampCubeShadow;
layout(set = 3, binding = 0) uniform samplerCubeArrayShadow     sampCubeArrayShadow;

layout(set = 4, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    float texel = 0.0;

    sparseTextureGradOffsetClampARB(samp2DShadow[index], vec3(0.1), vec2(0.2), vec2(0.3), ivec2(2), lodClamp, texel);
    fragColor.x += texel;

    sparseTextureGradOffsetClampARB(samp2DArrayShadow, vec4(0.1), vec2(0.2), vec2(0.3), ivec2(2), lodClamp, texel);
    fragColor.w += texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
