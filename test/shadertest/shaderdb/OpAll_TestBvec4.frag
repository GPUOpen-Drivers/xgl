#version 450

layout(binding = 0) uniform Uniforms
{
    bvec4 b4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    if (all(b4) == true)
    {
        color = vec4(1.0);
    }

    fragColor = color;
}