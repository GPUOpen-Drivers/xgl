#version 450 core

layout(location = 0) in vec4 fIn;
layout(location = 0, xfb_buffer = 1, xfb_offset = 24) out vec3 fOut1;
layout(location = 1, xfb_buffer = 0, xfb_offset = 16) out vec2 fOut2;

void main()
{
    fOut1 = fIn.xyz;
    fOut2 = fIn.xy;
}