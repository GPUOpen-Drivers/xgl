#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_1;
    vec3 f3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 f3_0 = cross(f3_1, f3_2);

    fragColor = (f3_0.x > 0.0) ? vec4(0.5) : vec4(1.0);
}