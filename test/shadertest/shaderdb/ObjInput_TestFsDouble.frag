#version 450

layout(location = 2) flat in dvec4 d4;
layout(location = 4) flat in dvec3 d3[2];
layout(location = 8) flat in dmat4 dm4;

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec4 d = d4;
    d += dvec4(d3[i], 1.0);
    d += dm4[1];

    fragColor = vec4(d);
}