#version 450

layout(set = 0, binding = 0) uniform sampler2DShadow      samp2DShadow;
layout(set = 1, binding = 0) uniform sampler2DArrayShadow samp2DArrayShadow[4];
layout(set = 0, binding = 1) uniform sampler2DRectShadow  samp2DRectShadow;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const ivec2 i2[4] = { ivec2(1), ivec2(2), ivec2(3), ivec2(4) };

    vec4 f4 = textureGatherOffsets(samp2DShadow, vec2(0.1), 0.9, i2);
    f4 += textureGatherOffsets(samp2DArrayShadow[index], vec3(0.2), 0.8, i2);
    f4 += textureGatherOffsets(samp2DRectShadow, vec2(1.0), 0.7, i2);

    fragColor = f4;
}