#version 450 core

layout(vertices = 3) out;

layout(location = 0) in vec2 f2[];
layout(location = 0, component = 2) in float f1[];

void main(void)
{
    vec3 f3 = vec3(f1[gl_InvocationID], f2[gl_InvocationID]);
    gl_out[gl_InvocationID].gl_Position = vec4(f3, 1.0);
}