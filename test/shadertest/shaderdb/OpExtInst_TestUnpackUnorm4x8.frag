#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = unpackUnorm4x8(u1);

    fragColor = (f4.x != f4.y) ? vec4(0.0) : vec4(1.0);
}