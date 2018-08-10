#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0, f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (f3_0 != f3_1) ? vec4(1.0) : vec4(0.5);
}