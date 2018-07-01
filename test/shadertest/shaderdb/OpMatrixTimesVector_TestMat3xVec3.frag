#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0;
    mat3 m3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    vec3 f3_1 = m3 * f3_0;

    fragColor = (f3_0 == f3_1) ? vec4(1.0) : color;
}