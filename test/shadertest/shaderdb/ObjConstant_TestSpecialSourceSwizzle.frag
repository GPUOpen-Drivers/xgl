#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    const float f1 = 0.2;
    const vec4 f4 = vec4(0.1, 0.9, 0.2, 1.0);
    fragColor[0] = f1;
    fragColor += f4;

    const double d1 = 0.25;
    const dvec4 d4_0 = dvec4(0.1, 0.8, 0.25, 0.2);
    dvec4 d4_1 = d4_0;
    d4_1[1] = d1;
    fragColor += vec4(d4_1);
}
 