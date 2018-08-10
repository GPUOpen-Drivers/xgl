#version 450 core

struct S
{
    vec4 f4;
};

layout(std430, binding = 0) buffer Block
{
    vec3 f3;
    S    s;
} block;

void main()
{
    S s;
    s.f4 = vec4(1.0);

    block.s = s;

    gl_Position = vec4(1.0);
}
