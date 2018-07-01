#version 450 core

layout(location = 2) in mat4 m4;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    gl_Position = m4[i];
}