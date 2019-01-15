#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2DShadow        samp2DShadow[4];
layout(set = 1, binding = 0) uniform samplerCubeArrayShadow sampCubeArrayShadow;
layout(set = 2, binding = 0) uniform sampler2DRectShadow    samp2DRectShadow;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    float texel = 0.0;

    sparseTextureARB(samp2DShadow[0], vec3(0.1), texel, 1.0);
    fragColor.x += texel;

    sparseTextureARB(sampCubeArrayShadow, vec4(0.3), 2.0, texel);
    fragColor.y += texel;

    sparseTextureOffsetARB(samp2DShadow[index], vec3(0.2), ivec2(1), texel, 1.0);
    fragColor.z += texel;

    sparseTextureOffsetARB(samp2DRectShadow, vec3(0.2), ivec2(1), texel);
    fragColor.w += texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
