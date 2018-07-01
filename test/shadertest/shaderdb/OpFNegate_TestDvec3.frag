#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3_1 = -d3_0;

    fragColor = vec4(d3_1, 0.5);
}