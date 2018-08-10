#version 450

layout(binding = 0) uniform Uniforms
{
    dvec4 d4;
    dmat4x2 dm4x2;
    dvec2 d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_0 = dm4x2 * d4;

    fragColor = (d2_0 == d2_1) ? vec4(1.0) : color;
}