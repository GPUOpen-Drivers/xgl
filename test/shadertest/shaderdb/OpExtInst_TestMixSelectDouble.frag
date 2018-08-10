#version 450

layout(binding = 0) uniform Uniforms
{
    double d1_1, d1_2;
    bool b1;

    dvec3 d3_1, d3_2;
    bvec3 b3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = mix(d1_1, d1_2, b1);

    dvec3 d3_0 = mix(d3_1, d3_2, b3);

    fragColor = (d3_0.y == d1_0) ? vec4(0.0) : vec4(1.0);
}