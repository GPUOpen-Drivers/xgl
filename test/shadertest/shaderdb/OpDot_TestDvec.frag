#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3_0, d3_1;
    dvec4 d4_0, d4_1;
    dvec2 d2_0, d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    double d1 = dot(d3_0, d3_1);
    d1 += dot(d4_0, d4_1);
    d1 += dot(d2_0, d2_1);

    fragColor = (d1 > 0.0) ? vec4(1.0) : color;
}