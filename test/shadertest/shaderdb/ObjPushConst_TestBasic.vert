#version 450 core

layout(push_constant) uniform PCB
{
    vec4 m1;
} g_pc;

void main()
{
    gl_Position = g_pc.m1;
}

