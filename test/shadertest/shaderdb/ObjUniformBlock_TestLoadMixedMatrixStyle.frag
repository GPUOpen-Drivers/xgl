#version 450 core

layout(std140, binding = 0, column_major) uniform Block
{
    vec3   f;
    layout(row_major) mat2x3 m0;
    mat2x3 m1;
} block;

layout(location = 0) out vec3 f;

void main()
{
    f  = block.f;
    f += block.m0[1];
    f += block.m1[0];
}