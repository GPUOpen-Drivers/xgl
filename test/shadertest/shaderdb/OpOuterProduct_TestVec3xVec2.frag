#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
    vec3 f3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat2x3 m2x3 = outerProduct(f3, f2);

    fragColor = (m2x3[0] != m2x3[1]) ? vec4(1.0) : color;
}