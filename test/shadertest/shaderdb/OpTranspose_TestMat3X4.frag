
#version 450 core

layout(location = 1) in vec4 inv;
layout(location = 0) out vec4 color;


void main()
{
    mat3x4 m2 = mat3x4(inv, inv, inv);
    mat4x3 m = transpose(m2);
    color.xyz = m[0] + m[1] + m[2] + m[3];
}