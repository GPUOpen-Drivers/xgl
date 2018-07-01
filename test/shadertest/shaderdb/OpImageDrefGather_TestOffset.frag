#version 450 core

layout(set = 0, binding = 0) uniform sampler2DShadow samp2DS;
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 oColor;

void main()
{
    ivec2 offset = ivec2(inUV);
    oColor = vec4(textureGatherOffset(samp2DS, inUV, 2, offset));
}

