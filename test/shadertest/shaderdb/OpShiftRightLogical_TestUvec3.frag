#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3 = uvec3(0);
    u3 = u3 >> 5;

    fragColor = (u3.x != 4) ? vec4(1.0) : vec4(0.5);
}