#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 0) out vec4 color;

void main()
{
    int iv = int(a0.x);
    color = vec4(findMSB(iv));
}