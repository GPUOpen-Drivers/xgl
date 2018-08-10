#version 450 core

layout(std140, binding = 0) uniform Block
{
    float f1[2];
    int  i;
} block;

void main()
{
    int i = block.i;
    float f1[2] = block.f1;
    gl_Position = vec4(f1[i]);
}