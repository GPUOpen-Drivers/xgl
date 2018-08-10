#version 450

layout(binding = 0) uniform Uniforms
{
    uvec3 u3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3 = u3;

    fragColor = (f3.x > 1.0) ? vec4(0.0) : vec4(1.0);
}