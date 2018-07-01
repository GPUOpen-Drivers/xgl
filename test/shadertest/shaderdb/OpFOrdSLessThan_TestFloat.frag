
#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    bvec4 x = lessThan (ivec4(colorIn1), ivec4(colorIn2));
    color = vec4(x);
}