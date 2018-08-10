#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec3 f3_1, f3_2, f3_3;
    vec4 f4_1, f4_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3_0 = mix(f3_1, f3_2, f3_3);

    vec4 f4_0 = mix(f4_1, f4_2, f1_1);

    fragColor = (f3_0.y == f4_0.x) ? vec4(0.0) : vec4(1.0);
}