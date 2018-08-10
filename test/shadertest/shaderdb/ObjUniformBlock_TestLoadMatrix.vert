#version 450 core

layout(std140, binding = 0) uniform Block
{
    int  i;
    mat4 m4;
} block;

void main()
{
    int i = block.i;
    mat4 m4 = block.m4;
    gl_Position = m4[i];
}