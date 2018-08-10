#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    color = vec4(bitfieldInsert(3, 6, 4, 5));
    // output 0x42c60000
}