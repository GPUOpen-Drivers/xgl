#version 450

layout(binding = 0) uniform Uniforms
{
    uvec3 u3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3 = u3;

    fragColor = (d3.x > d3.z) ? vec4(0.0) : vec4(1.0);
}