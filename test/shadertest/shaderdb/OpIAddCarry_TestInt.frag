#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    uvec4 bd = uvec4(colorIn1);
    uint outi = 0;
    uint out1 = uaddCarry(bd.x,bd.y,outi);
    color = vec4(out1 + outi);
}