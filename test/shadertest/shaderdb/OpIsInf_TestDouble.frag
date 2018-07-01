#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 f;

void main()
{
    f = (isinf(d1)) ? vec4(0.0) : vec4(1.0);
}