#version 450

layout(binding = 0) uniform Uniforms
{
    mat3 m;
};

mat3 m2;
layout(location = 1) out vec3 o1;

void main()
{
    m2 = m;
    o1 = m2[2];
}