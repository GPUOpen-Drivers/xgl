#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec4 i4 = ivec4(0);
    i4 = i4 >> 5;

    fragColor = (i4.x != 4) ? vec4(1.0) : vec4(0.5);
}