#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2DShadow        samp2DShadow[4];
layout(set = 1, binding = 0) uniform sampler2DRectShadow    samp2DRectShadow;

layout(set = 2, binding = 0) uniform Uniforms
{
    int   index;
};

const ivec2 offsets[4] = { ivec2(1), ivec2(2), ivec2(3), ivec2(4) };

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseTextureGatherARB(samp2DShadow[index], vec2(0.1), 1.0, texel);
    fragColor += texel;

    sparseTextureGatherARB(samp2DRectShadow, vec2(1.1), 1.1, texel);
    fragColor += texel;

    sparseTextureGatherOffsetARB(samp2DRectShadow, vec2(0.1), 1.5, ivec2(1), texel);
    fragColor += texel;

    sparseTextureGatherOffsetsARB(samp2DRectShadow, vec2(1.2), 1.1, offsets, texel);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
