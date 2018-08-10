#version 450

layout(set = 0, binding = 0) uniform sampler1D  samp1D;
layout(set = 1, binding = 0) uniform sampler2D  samp2D[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = textureProjOffset(samp1D, vec2(0.5), 3, 0.4);
    f4 += textureProjOffset(samp2D[index], vec3(0.6), ivec2(7));

    fragColor = f4;
}