#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 i2 = ivec2(0);
    i2 = i2 << 3;

    fragColor = (i2.x != 4) ? vec4(1.0) : vec4(0.5);
}