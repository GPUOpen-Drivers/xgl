#version 450

layout(set = 0, binding = 0) uniform sampler1DShadow    samp1DShadow;
layout(set = 1, binding = 0) uniform sampler2DShadow    samp2DShadow[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1 = textureProj(samp1DShadow, vec4(0.8), 1.0);
    f1 += textureProj(samp2DShadow[index], vec4(0.5));

    fragColor = vec4(f1);
}