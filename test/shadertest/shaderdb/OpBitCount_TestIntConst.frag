#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    color = vec4(bitCount(bd.x) + bitCount(384));
}