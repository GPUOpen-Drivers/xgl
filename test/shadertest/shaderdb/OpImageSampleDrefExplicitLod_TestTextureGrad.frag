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
    float f1 = textureGrad(samp1DShadow, vec3(0.8), 0.6, 0.7);
    f1 += textureGrad(samp2DShadow[index], vec3(0.5), vec2(0.4), vec2(0.3));

    fragColor = vec4(f1);
}