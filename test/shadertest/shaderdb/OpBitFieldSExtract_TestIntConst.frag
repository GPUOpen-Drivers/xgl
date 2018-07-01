#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    int b0 = bitfieldExtract(bd.x, bd.x, bd.y);
    int b1 = bitfieldExtract(3423, 0, 32);
    color = vec4(b0 + b1);
}