#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
    dmat4  dm4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dmat4 dm4_0 = d1 * dm4;
    fragColor = vec4(dm4_0[1]);
}