#version 450

layout(set = 0, binding = 0) uniform sampler2D samp2D[4];

layout(set = 1, binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

vec4 func(sampler2D s2D, vec2 coord)
{
    return texture(s2D, coord);
}

void main()
{
    vec4 color = func(samp2D[i], vec2(0.5));
    color += func(samp2D[1], vec2(1.0));

    fragColor = color;
}