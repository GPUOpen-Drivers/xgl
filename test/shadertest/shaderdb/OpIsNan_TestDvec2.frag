#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2;
};

layout(location = 0) out vec4 f;

void main()
{
    bvec2 b2 = isnan(d2);

    f = (b2.x) ? vec4(0.0) : vec4(1.0);
}