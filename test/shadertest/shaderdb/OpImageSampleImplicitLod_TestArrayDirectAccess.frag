#version 450 core

layout(set = 0, binding = 0) uniform sampler2D samp2D[4];
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = texture(samp2D[2], vec2(1, 1));
}

