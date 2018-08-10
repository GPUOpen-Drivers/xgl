#version 450 core

layout(location = 3) out Block
{
    int  i1;
    vec3 f3;
    mat4 m4;
} block;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    block.i1 = 2;
    block.f3 = vec3(1.0);
    block.m4[i] = vec4(0.5);
}