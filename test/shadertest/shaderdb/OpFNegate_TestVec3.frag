#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3_1 = -f3_0;

    fragColor = vec4(f3_1, 0.5);
}