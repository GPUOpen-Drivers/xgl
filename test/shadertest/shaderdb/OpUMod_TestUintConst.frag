#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    uint ua = uint(a0.x);
    uint ub = uint(b0.x);
    uint uc = ua % ub;
    uint ucc = 10 % 3;
    color = vec4(uc + ucc);
}