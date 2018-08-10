#version 450

layout(binding = 0) uniform Uniforms
{
    mat2x3 m2x3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dmat2x3 dm2x3 = m2x3;

    fragColor = (dm2x3[0] != dm2x3[1]) ? vec4(0.0) : vec4(1.0);
}