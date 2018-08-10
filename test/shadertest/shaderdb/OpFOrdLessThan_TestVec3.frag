#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3_0, f3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    bvec3 b3 = lessThan(f3_0, f3_1);

    fragColor = (b3.x) ? vec4(1.0) : vec4(0.5);
}