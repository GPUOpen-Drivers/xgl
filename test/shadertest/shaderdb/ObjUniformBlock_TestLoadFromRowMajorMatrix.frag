#version 450 core

layout(std140, binding = 0, row_major) uniform Block
{
    vec3   f;
    mat2x3 m2x3;
} block;

layout(location = 0) out vec3 f;

void main()
{
    f  = block.f;
    f += block.m2x3[1];
    f += vec3(block.m2x3[0][2]);
}