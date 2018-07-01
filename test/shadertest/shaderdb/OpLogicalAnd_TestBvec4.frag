#version 450

layout(binding = 0) uniform Uniforms
{
    bvec4 b4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (b4.x && b4.y) ? vec4(1.0) : vec4(0.5);
}