#version 450

layout(binding = 0) uniform Uniforms
{
    mat2x3 m2x3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat3x2 m3x2 = transpose(m2x3);

    fragColor = (m3x2[0] == vec2(0.5)) ? vec4(1.0) : color;
}