#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    vec3 f3 = vec3(0.0);
    const float f1 = 1.0;
    f3 *= f1;

    color.xyz = f3;

    fragColor = color;
}