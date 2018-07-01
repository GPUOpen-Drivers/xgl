#version 450 core

layout(std430, binding = 0) buffer Block
{
    uint  u1;
    uvec2 u2;
    uvec3 u3;
    uvec4 u4;
} block;

void main()
{
    block.u1 += 1;
    block.u2 += uvec2(2);
    block.u3 += uvec3(3);
    block.u4 += uvec4(4);

    gl_Position = vec4(1.0);
}