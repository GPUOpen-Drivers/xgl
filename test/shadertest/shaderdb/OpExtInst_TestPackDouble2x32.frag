#version 450

layout(binding = 0) uniform Uniforms
{
    uvec2 u2;
    double d1_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1_0 = packDouble2x32(u2);

    fragColor = (d1_0 != d1_1) ? vec4(0.0) : vec4(1.0);
}