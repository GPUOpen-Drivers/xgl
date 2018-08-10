#version 450 core

layout(binding = 0) uniform Uniforms
{
    int limit;
};

layout(location = 0) out vec4 f;

void main()
{
    vec4 f4 = vec4(0.0);

    int i = 0;
    while (i < limit)
    {
        f4 += vec4(0.5);
        i++;
    }

    f = f4;
}