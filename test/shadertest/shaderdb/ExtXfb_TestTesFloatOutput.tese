#version 450 core

layout(triangles) in;

layout(location = 0) in vec4 fIn[];
layout(location = 0, xfb_buffer = 1, xfb_offset = 24) out vec3 fOut1;
layout(location = 2, xfb_buffer = 0, xfb_offset = 16) out vec2 fOut2;

void main(void)
{
    fOut1 = fIn[0].xyz + fIn[1].xyz + fIn[2].xyz;
    fOut2 = fIn[0].xy + fIn[1].xy + fIn[2].xy;
}