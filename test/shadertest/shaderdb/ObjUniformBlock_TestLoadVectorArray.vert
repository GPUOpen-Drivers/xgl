#version 450 core

layout(std140, binding = 0) uniform Block
{
    int  i;
    vec4 f4[4];
} block;

void main()
{
    int i = block.i;
    vec4 f4[4] = block.f4;
    gl_Position = f4[i];
}