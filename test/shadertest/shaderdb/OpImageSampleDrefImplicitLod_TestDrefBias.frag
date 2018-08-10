#version 450 core

layout(set = 0, binding = 0) uniform sampler2DShadow samp2DS;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(texture(samp2DS, vec3(0, 0, 1), 1));
}

