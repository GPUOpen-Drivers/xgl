#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    int ua = int(a0.x);
    int ub = int(b0.x);
    int uc = ua % ub;
    int uc0 = 10 % -5;
    int uc1 = 10 % 5;
    int uc2 = 10 % 3;

    color = vec4(uc + uc0 + uc1 + uc2);
}