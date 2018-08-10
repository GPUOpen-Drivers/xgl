#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec2 u2 = unpackDouble2x32(d1);

    fragColor = (u2.x != 5) ? vec4(0.0) : vec4(1.0);
}