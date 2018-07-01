#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    bvec4 x = lessThan (uvec4(colorIn1), uvec4(colorIn2));
    color = vec4(x);
}