#version 450 core

layout(triangles) in;

layout(location = 0) out vec2 f2;
layout(location = 0, component = 2) out float f1;

void main(void)
{
    vec3 f3 = gl_in[1].gl_Position.xyz;
    f2 = f3.yz;
    f1 = f3.x;
}
