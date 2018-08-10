#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 f2 = unpackSnorm2x16(u1);

    fragColor = (f2.x != f2.y) ? vec4(0.0) : vec4(1.0);
}