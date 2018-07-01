#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1 = packSnorm2x16(f2);

    fragColor = (u1 != 5) ? vec4(0.0) : vec4(1.0);
}