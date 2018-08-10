#version 450 core

layout(binding = 0) uniform Uniforms
{
    bool cond;
};

layout(location = 0) out vec4 f;

void main()
{
    if (cond)
    {
        f = vec4(1.0);
        return;
    }

    f = vec4(1.5);
}