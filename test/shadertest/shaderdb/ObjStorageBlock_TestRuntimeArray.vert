#version 450 core

layout(std430, binding = 0) buffer Block
{
    vec4   base;
    vec4   o[];
} block;

void main()
{
    block.o[gl_VertexIndex] = block.base + vec4(gl_VertexIndex);

    gl_Position = vec4(1.0);
}