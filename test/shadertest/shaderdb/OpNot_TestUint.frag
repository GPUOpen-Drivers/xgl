
#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{

    uvec4 bd = uvec4(colorIn1.x);
    uint bi = ~(bd.x) | ~(1);
    color = vec4(bi);
}