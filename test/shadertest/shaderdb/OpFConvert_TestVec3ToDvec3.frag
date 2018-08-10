#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 v3;
    dvec3 d3_0;
};

layout(location = 0) out vec4 fragColor;

void main ()
{
    vec4 color = vec4(0.5);

    dvec3 d3_1 = v3;

    if (d3_0 == d3_1)
    {
        color = vec4(1.0);
    }

    fragColor = vec4(1.0);
}