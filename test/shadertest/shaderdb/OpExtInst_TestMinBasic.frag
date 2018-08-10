#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fmin = min(a,b);
    ivec4 imin = min(ivec4(a), ivec4(b));
    uvec4 umin = min(uvec4(a), uvec4(b));
    frag_color = fmin + vec4(imin) + vec4(umin);
}