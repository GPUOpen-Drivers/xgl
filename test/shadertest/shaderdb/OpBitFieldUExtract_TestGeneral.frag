#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1;
    uvec3 u3_1;

    int i1_1, i1_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_0 = bitfieldExtract(u1_1, i1_1, i1_2);

    uvec3 u3_0 = bitfieldExtract(u3_1, i1_1, i1_2);

    fragColor = (u1_0 != u3_0.x) ? vec4(0.0) : vec4(1.0);
}