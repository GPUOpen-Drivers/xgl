#version 450 core

layout(location = 0) in vec3 l0 ;
layout(location = 1) in vec3 l1 ;
layout(location = 2) in vec3 l2 ;
layout(location = 3) in vec3 l3 ;


layout(location = 0) out vec4 color;

void main()
{
    mat3 x = mat3(l0, l1, l2);
    mat3 y = mat3(l1, l2, l3);
    mat3 z = x * y;
    color.xyz = z[0] + z[1] + z[2];
}

