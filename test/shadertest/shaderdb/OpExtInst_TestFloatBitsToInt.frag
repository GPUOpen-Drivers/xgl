#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec3 i3 = floatBitsToInt(f3);

    fragColor = (i3.x != i3.y) ? vec4(0.0) : vec4(1.0);
}