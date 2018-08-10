#version 450 core

layout(std430, binding = 0, column_major) buffer Block
{
    vec3   f;
    layout(row_major) mat2x3 m0;
    mat2x3 m1;
} block;

layout(location = 0) out vec3 f;

void main()
{
    block.m0[1] = block.f;
    block.m1[0] = block.f;
}