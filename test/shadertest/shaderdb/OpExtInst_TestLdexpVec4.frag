#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    ivec4 fo = ivec4(b);
    vec4 fv = ldexp(a, fo);
    frag_color = fv;
}