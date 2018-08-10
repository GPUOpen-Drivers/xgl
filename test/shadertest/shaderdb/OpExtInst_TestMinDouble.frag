#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1, d1_2;
    dvec3 d3_1, d3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = min(d1_1, d1_2);

    dvec3 d3_0 = min(d3_1, d3_2);

    fragColor = (d1_0 != d3_0.x) ? vec4(0.0) : vec4(1.0);
}