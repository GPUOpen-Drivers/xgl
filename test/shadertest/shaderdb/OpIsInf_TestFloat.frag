#version 450

layout(binding = 0) uniform Uniforms
{
    float f1;
};

layout(location = 0) out vec4 f;

void main()
{
    f = (isinf(f1)) ? vec4(0.0) : vec4(1.0);
}