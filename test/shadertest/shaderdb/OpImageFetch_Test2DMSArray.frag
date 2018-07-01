#version 450 core

layout(set = 0, binding = 0) uniform sampler2DMSArray samp;
layout(location = 0) in vec3 inUV;
layout(location = 0) out vec4 oColor;

void main()
{
    ivec3 iUV = ivec3(inUV);
    oColor = texelFetch(samp, iUV, 2);
}

