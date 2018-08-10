#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3;
    double d1_1;

    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    double d1_0 = d3.zyx[i];

    if (d1_0 == d1_1)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}