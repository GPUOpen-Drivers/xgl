#version 450 core

layout(location = 0, component = 0) out float f1;
layout(location = 0, component = 2) out vec2 f2;

void main()
{
    f1 = 2.0;
    f2 = vec2(4.0, 5.0);
}