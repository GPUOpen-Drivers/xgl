#version 450 core

layout(set = 0, binding = 0) uniform sampler2D samp;
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 oColor;

void main()
{
    ivec2 offset = ivec2(inUV);
    oColor = textureGatherOffset(samp, inUV, offset, 2);
}

