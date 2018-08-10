#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = fract(f1_1);

    vec3 f3_0 = fract(f3_1);

    fragColor = ((f1_0 != f3_0.x)) ? vec4(0.0) : vec4(1.0);
}