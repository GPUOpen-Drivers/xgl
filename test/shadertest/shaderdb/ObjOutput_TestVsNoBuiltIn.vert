#version 450 core

layout(location = 1) in vec4 f4;
layout(location = 2) out float f1;

void main()
{
    f1 = f4.x;
}