#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    uvec4 u4 = uvec4(5);
    uint u1 = 90;
    u4 *= u1;

    fragColor = (u4 == uvec4(6)) ? vec4(1.0) : color;
}