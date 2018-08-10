#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fstep = step(a, b);
    frag_color = fstep + step(vec4(0.7), vec4(0.2));
}