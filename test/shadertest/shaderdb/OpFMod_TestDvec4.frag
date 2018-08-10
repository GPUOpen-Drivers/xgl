#version 450

layout(binding = 0) uniform Uniforms
{
    dvec4  d4_1;
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec4 d4_0 = dvec4(0.0);
    d4_0 = mod(d4_0, d4_1);

    d4_0 += mod(d4_0, d1);

    fragColor = (d4_0.y > 0.0) ? vec4(0.0) : vec4(1.0);
}