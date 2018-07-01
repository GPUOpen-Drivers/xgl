#version 450 core

layout(vertices = 3) out;

layout(location = 0) out vec2 f2[];
layout(location = 0, component = 2) out float f1[];

void main(void)
{
    vec4 f4 = gl_in[gl_InvocationID].gl_Position;
    f2[gl_InvocationID] = f4.xy;
    f1[gl_InvocationID] = f4.w;
}