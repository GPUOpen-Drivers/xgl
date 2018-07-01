#version 450

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    if (i > 0)
    {
        discard;
    }

    fragColor = vec4(1.0);
}