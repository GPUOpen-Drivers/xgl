#version 450 core

layout(location = 2) in vec4  f4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    gl_Position = f4[i];
}