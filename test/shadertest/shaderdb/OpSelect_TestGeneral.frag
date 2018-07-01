#version 450

layout(binding = 0) uniform Uniforms
{
    bool cond;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(cond);
}