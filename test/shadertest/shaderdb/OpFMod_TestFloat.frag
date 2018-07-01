#version 450 core

layout(location = 0) in float a;
layout(location = 1) in vec4 b0;


layout(location = 0) out vec4 frag_color;
void main()
{
    frag_color = vec4(mod(b0.x,a));
}