#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1 = packUnorm4x8(f4);

    fragColor = (u1 != 5) ? vec4(0.0) : vec4(1.0);
}