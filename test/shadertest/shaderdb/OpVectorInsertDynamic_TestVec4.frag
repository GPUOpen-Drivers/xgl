#version 450

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);
    color[i] = 0.4;

    fragColor = color;
}