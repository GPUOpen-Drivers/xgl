#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    ivec2 i2 = ivec2(0, 0);
    int i1 = 1;
    i2 *= i1;

    fragColor = (i2 != ivec2(1)) ? vec4(1.0) : color;
}