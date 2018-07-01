#version 450 core

layout(set = 0, binding = 0) uniform samplerCubeShadow samp2DS;
layout(location = 0) in vec4 inUV;
layout(location = 0) out vec4 oColor;

void main()
{
    oColor = vec4(texture(samp2DS, inUV));
}

