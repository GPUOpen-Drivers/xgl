#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    // FrexpStruct
    ivec4 fo = ivec4(0);
    vec4 fv = frexp(a, fo);
    frag_color = vec4(fv * fo);
}