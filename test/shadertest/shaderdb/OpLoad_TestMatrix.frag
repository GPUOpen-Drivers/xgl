#version 450

layout(set  = 0, binding = 0) uniform Uniforms
{
    mat3 m;
    int index;
};

layout(set = 1, binding = 1) uniform BB
{
    mat3 m2;
    layout (row_major) mat2x3 m3;
};

layout(location = 0) out vec3 o1;
layout(location = 1) out vec3 o2;
layout(location = 2) out vec3 o3;

void main()
{
    o1 = m[2];
    o2 = m2[2] + m3[1];
    o3 = m2[index] + m3[index];
}