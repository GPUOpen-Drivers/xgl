#version 450

layout(binding = 0) uniform Uniforms
{
    mat3 m3_1, m3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    mat3 m3_0 = matrixCompMult(m3_1, m3_2);

    fragColor = (m3_0[0] != m3_0[1]) ? vec4(0.0) : vec4(1.0);
}