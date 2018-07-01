#version 450 core

layout(set = 0, binding = 0) uniform sampler2D samp;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = textureLod(samp, vec2(0, 0), 1);
}

