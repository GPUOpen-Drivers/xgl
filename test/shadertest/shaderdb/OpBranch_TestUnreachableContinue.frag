#version 450 core

layout(binding = 0) uniform Uniforms
{
    int limit;
};

layout(location = 0) out vec4 f;

void main()
{
    vec4 color = vec4(0.0);

    for (uint i = 0; i < limit; i++)
    {
        color += vec4(0.5);
        break;
    }

    f = color;
}