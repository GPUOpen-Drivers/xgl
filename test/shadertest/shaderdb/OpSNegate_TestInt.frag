#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_1 = -i1_0;

    fragColor = (i1_1 == 5) ? vec4(0.0) : vec4(1.0);
}