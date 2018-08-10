#version 450 core

layout(std140, binding = 0) uniform Block
{
    uint  u1;
    uvec2 u2;
    uvec3 u3;
    uvec4 u4;
} block;

void main()
{
    uvec4 u4 = block.u4;
    u4.xyz += block.u3;
    u4.xy  += block.u2;
    u4.x   += block.u1;

    gl_Position = vec4(u4);
}