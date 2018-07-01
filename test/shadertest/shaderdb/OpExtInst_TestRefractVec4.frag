#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 2) in vec4 c0;
layout(location = 0) out vec4 color;

void main()
{
    vec2 a = a0.xy;
    vec2 b = b0.xy;
    color = vec4(refract(a0, b0, c0.x));
}