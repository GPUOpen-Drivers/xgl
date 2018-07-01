#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2D      samp2D[4];
layout(set = 1, binding = 0) uniform sampler2DRect  samp2DRect;
layout(set = 2, binding = 0) uniform sampler2DMS    samp2DMS;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseTexelFetchARB(samp2D[index], ivec2(1), 1, texel);
    fragColor += texel;

    sparseTexelFetchARB(samp2DMS, ivec2(1), 2, texel);
    fragColor += texel;

    sparseTexelFetchARB(samp2DRect, ivec2(2), texel);
    fragColor += texel;

    sparseTexelFetchOffsetARB(samp2DRect, ivec2(2), ivec2(3), texel);
    fragColor += texel;
}