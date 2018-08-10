#version 450

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform Uniforms
{
    dvec4 d4;
    int index;
};

void main()
{
    double d1 = d4[index];
    d1 += d4[2];
    fragColor = vec4(float(d1));
}