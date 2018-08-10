#version 450

layout(location = 2) in vec4 f4;
layout(location = 5) flat in int i1;
layout(location = 7) flat in uvec2 u2;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = f4;
    f += vec4(i1);
    f += vec4(u2, u2);

    fragColor = f;
}