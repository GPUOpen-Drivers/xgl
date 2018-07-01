#version 450

layout(binding = 0) uniform Uniforms
{
    dmat4x3 dm4x3;
    dmat3x4 dm3x4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dmat3 dm3 = dm4x3 * dm3x4;

    fragColor = (dm3[0] == dm3[1]) ? vec4(1.0) : color;
}