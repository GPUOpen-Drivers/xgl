#version 450 core

layout(location = 3) out mat4 m4[2];

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    m4[1][i] = vec4(1.0);
}

