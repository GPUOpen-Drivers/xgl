#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 2) in vec4 c;

layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 f = smoothstep(a, b, c);
    frag_color = f + smoothstep(vec4(0.7), vec4(0.2), vec4(0.1));
}
