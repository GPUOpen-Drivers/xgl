#version 450 core

layout(std430, binding = 0, row_major) buffer Block
{
    vec3   f;
    mat2x3 m2x3;
} block;

void main()
{
    block.m2x3[1] = block.f;
    block.m2x3[0][2] = 3.0;
}
