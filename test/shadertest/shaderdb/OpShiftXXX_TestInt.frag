#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 ia = ivec4(a0);
    int x = ((ia.x >> ia.y) & ia.z | (ia.w)) << (ia.x);
    ivec4 ca = ivec4(10, 4, -3, -9);
    int y = ((ca.x >> ca.y) & ca.z | (ca.w)) << (ca.x);
    color = vec4(x + y);
}