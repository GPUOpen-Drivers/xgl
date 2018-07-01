#version 450 core

layout(constant_id = 2) const int  sci = -2;

const ivec3 iv3 = ivec3(sci, -7, -8);
const ivec2 iv2 = iv3.yx;
const int   i   = iv2[1];

layout(location = 0) out vec4 f3;

void main()
{
    f3 = vec4(i);
}