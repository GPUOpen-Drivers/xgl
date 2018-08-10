#version 450

layout(binding = 0) uniform Uniforms
{
    ivec4 i4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec4 d4 = i4;

    fragColor = (d4.x > d4.z) ? vec4(0.0) : vec4(1.0);
}