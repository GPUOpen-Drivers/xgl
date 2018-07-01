#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2_0, i2_1;
    uvec2 u2_0, u2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    bvec2 b2;

    b2 = lessThanEqual(i2_0, i2_1);

    b2 = lessThanEqual(u2_0, u2_1);

    fragColor = (b2.x) ? vec4(1.0) : vec4(0.5);
}