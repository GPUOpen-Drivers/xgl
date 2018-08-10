#version 450

layout(binding = 0) uniform Uniforms
{
    ivec3 i3_1, i3_2;
    uvec2 u2_1, u2_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec3 i3_0 = i3_1 * i3_2;

    uvec2 u2_0 = u2_1 * u2_2;

    fragColor = ((i3_0.y != 5) && (u2_0.x != 7)) ? vec4(0.0) : vec4(1.0);
}