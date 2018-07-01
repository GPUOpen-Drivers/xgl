#version 450 core

layout(location = 0, component = 0) in float f1;
layout(location = 0, component = 1) in vec2  f2;

layout(location = 0) out vec3 color;

void main()
{
    vec3 f3 = vec3(f1, f2);
    color = f3;
}