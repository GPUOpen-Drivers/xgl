#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2_1;
    double d1;

    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_0 = dvec2(0.0);
    d2_0[i] = d1;

    if (d2_0 == d2_1)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}