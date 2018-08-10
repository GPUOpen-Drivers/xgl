#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
    mat3x2 m3x2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    vec3 f3 = f2 * m3x2;

    fragColor = (f3 != vec3(0.5)) ? vec4(1.0) : color;
}