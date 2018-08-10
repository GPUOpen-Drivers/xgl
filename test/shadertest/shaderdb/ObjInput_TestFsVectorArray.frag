#version 450

layout(location = 2) flat in vec4 f4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = f4[i];
}