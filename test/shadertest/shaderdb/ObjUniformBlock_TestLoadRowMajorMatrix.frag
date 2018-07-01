#version 450 core

layout(std140, binding = 0, row_major) uniform Block
{
    vec3   f;
    mat2x3 m2x3;
} block;

layout(location = 0) out vec3 f;

void main()
{
    mat2x3 m2x3 = block.m2x3;
    f = m2x3[1] + block.f;
}