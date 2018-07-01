#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
    vec4 f4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat4x2 m4x2 = outerProduct(f2, f4);

    fragColor = (m4x2[0] != m4x2[1]) ? vec4(1.0) : color;
}