#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2D      samp2D[4];
layout(set = 1, binding = 0) uniform sampler2DRect  samp2DRect;

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

    sparseTextureGatherARB(samp2D[index], vec2(0.1), texel);
    fragColor += texel;

    sparseTextureGatherARB(samp2DRect, vec2(1.1), texel, 3);
    fragColor += texel;

    sparseTextureGatherOffsetARB(samp2D[1], vec2(0.1), ivec2(1), texel);
    fragColor += texel;

    sparseTextureGatherOffsetARB(samp2DRect, vec2(1.1), ivec2(2), texel, 2);
    fragColor += texel;

    sparseTextureGatherOffsetsARB(samp2DRect, vec2(1.2), offsets, texel, 3);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
