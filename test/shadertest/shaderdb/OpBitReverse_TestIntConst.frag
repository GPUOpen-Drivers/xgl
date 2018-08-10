#version 450 core

layout(location = 0) out vec4 color;
void main()
{
    color = vec4(bitfieldReverse(342));
}