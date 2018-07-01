#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4_1;
    float f1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4_0 = vec4(0.0);
    f4_0 = mod(f4_0, f4_1);

    f4_0 += mod(f4_0, f1);

    fragColor = (f4_0.y > 0.0) ? vec4(0.0) : vec4(1.0);
}