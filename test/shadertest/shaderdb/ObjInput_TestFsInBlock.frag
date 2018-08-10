#version 450

layout(location = 2) in Block
{
    flat   int  i1;
    smooth vec3 f3;
    smooth mat4 m4;
} block;

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = vec4(block.i1);
    f += vec4(block.f3, 1.0);
    f += block.m4[i];

    fragColor = f;
}

