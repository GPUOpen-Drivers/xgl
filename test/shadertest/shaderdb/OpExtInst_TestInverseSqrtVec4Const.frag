#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fv = inversesqrt(a);
    fv.x = inversesqrt(2.0);
    frag_color = fv;
}