#version 450

layout (location = 0) out vec4 fragColor;

void main()
{
    vec3 f3 = vec3(0.0);
    f3 = dFdy(f3);
    f3 = dFdyFine(f3);
    f3 = dFdyCoarse(f3);

    fragColor = (f3[0] == f3[1]) ? vec4(1.0) : vec4(0.5);
}