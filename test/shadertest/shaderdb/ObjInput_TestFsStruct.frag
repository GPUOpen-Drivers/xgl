#version 450

struct S
{
    int  i1;
    vec3 f3;
    mat4 m4;
};

layout(location = 4) flat in S s;

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = vec4(s.i1);
    f += vec4(s.f3, 1.0);
    f += s.m4[i];

    fragColor = f;
}