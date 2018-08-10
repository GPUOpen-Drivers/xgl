#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2D      samp2D;
layout(set = 1, binding = 0) uniform sampler3D      samp3D[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseTextureLodARB(samp2D, vec2(0.1), 1.1, texel);
    fragColor += texel;

    sparseTextureLodARB(samp3D[index], vec3(0.2), 1.2, texel);
    fragColor += texel;

    sparseTextureLodOffsetARB(samp2D, vec2(0.3), 1.3, ivec2(2), texel);
    fragColor += texel;
}