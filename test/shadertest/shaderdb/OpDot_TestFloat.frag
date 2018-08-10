#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 10) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    vec2 a = a0.xy;
    vec2 b = b0.xy;
    color = vec4(dot(a0,b0));
}