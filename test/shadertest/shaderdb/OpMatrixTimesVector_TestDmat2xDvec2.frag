#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2_0;
    dmat2 dm2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_1 = dm2 * d2_0;

    fragColor = (d2_0 == d2_1) ? vec4(1.0) : color;
}