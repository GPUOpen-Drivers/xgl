#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1;
    float f1_0 = frexp(f1_1, i1);

    ivec3 i3;
    vec3 f3_0 = frexp(f3_1, i3);

    fragColor = ((f3_0.x != f1_0) || (i3.x != i1)) ? vec4(0.0) : vec4(1.0);
}