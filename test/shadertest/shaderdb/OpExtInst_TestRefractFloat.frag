#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    float f1_2;
    float f1_3;

    vec4 f4_1;
    vec4 f4_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = refract(f1_1, f1_2, f1_3);

    vec4 f4_0 = refract(f4_1, f4_2, f1_3);

    fragColor = (f4_0.x > f1_0) ? vec4(0.5) : vec4(1.0);
}