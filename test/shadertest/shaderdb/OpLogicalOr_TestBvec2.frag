#version 450

layout(binding = 0) uniform Uniforms
{
    bvec2 b2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (b2.x || b2.y) ? vec4(1.0) : vec4(0.5);
}