#version 450 core

struct S
{
    int  i1;
    vec3 f3;
    mat4 m4;
};

layout(location = 4) out S s;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    s.i1 = 2;
    s.f3 = vec3(1.0);
    s.m4[i] = vec4(0.5);
}