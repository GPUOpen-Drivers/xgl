#version 450 core

layout(std140, binding = 0) uniform Block
{
    mat4 m4;
    int  i;
} block;

void main()
{
    int i = block.i;
    gl_Position = block.m4[i] + block.m4[1];
}