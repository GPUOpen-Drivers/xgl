#version 450 core

layout(set = 0, binding = 0) uniform sampler3D samp;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = texture(samp, vec3(1, 2, 3));
}

