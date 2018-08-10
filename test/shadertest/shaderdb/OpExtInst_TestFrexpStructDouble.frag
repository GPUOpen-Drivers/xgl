#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    // FrexpStruct
    int fo = 0;
    double fv = frexp(double(a.x), fo);
    frag_color = vec4(fv * fo);
}

