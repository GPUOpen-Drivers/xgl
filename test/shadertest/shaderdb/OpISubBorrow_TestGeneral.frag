#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1, u1_2;
    uvec3 u3_1, u3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_0, u1_3;
    u1_0 = usubBorrow(u1_1, u1_2, u1_3);

    uvec3 u3_0, u3_3;
    u3_0 = usubBorrow(u3_1, u3_2, u3_3);

    fragColor = ((u1_0 != u1_3) || (u3_0 == u3_3)) ? vec4(0.0) : vec4(1.0);
}