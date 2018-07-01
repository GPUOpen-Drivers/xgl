#version 450 core

layout(location = 0) in vec4  f4;
layout(location = 1) in int   i1;
layout(location = 2) in uvec2 u2;

void main()
{
    vec4 f = f4;
    f += vec4(i1);
    f += vec4(u2, u2);

    gl_Position = f;
}