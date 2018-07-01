#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 i2_0 = ivec2(0);
    i2_0 %= i2_1;

    fragColor = (i2_0.y != 4) ? vec4(0.0) : vec4(1.0);
}