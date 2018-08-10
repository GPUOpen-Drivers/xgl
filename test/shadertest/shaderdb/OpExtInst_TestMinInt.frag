#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_1, i1_2;
    ivec3 i3_1, i3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_0 = min(i1_1, i1_2);

    ivec3 i3_0 = min(i3_1, i3_2);

    fragColor = (i1_0 != i3_0.x) ? vec4(0.0) : vec4(1.0);
}