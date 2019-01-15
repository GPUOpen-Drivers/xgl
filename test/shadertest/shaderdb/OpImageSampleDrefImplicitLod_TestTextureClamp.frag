#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler1DShadow            samp1DShadow[4];
layout(set = 1, binding = 0) uniform sampler2DShadow            samp2DShadow;
layout(set = 2, binding = 0) uniform samplerCubeShadow          sampCubeShadow;
layout(set = 3, binding = 0) uniform sampler1DArrayShadow       samp1DArrayShadow;
layout(set = 4, binding = 0) uniform sampler2DArrayShadow       samp2DArrayShadow;
layout(set = 5, binding = 0) uniform samplerCubeArrayShadow     sampCubeArrayShadow;

layout(set = 6, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);

    fragColor.x += textureClampARB(samp1DShadow[index], vec3(0.1), lodClamp);

    fragColor.x += textureClampARB(samp2DShadow, vec3(0.1), lodClamp);

    fragColor.x += textureClampARB(sampCubeShadow, vec4(0.1), lodClamp);

    fragColor.x += textureClampARB(samp1DArrayShadow, vec3(0.1), lodClamp);

    fragColor.x += textureClampARB(samp2DArrayShadow, vec4(0.1), lodClamp);

    fragColor.x += textureClampARB(sampCubeArrayShadow, vec4(0.1), 1.2, lodClamp);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
