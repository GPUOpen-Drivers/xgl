#version 450

layout(location = 2) flat in mat4 m4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = m4[i][i];
}