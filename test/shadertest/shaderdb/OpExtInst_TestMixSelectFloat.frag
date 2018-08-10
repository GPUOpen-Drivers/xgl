#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1, f1_2;
    bool b1;

    vec3 f3_1, f3_2;
    bvec3 b3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = mix(f1_1, f1_2, b1);

    vec3 f3_0 = mix(f3_1, f3_2, b3);

    fragColor = (f3_0.y == f1_0) ? vec4(0.0) : vec4(1.0);
}