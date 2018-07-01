#version 450

layout(location = 1) in Block
{
    flat int i1;
    centroid vec4 f4;
    noperspective sample mat4 m4;
} block;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = block.f4;
    f += block.m4[1];
    f += vec4(block.i1);

    fragColor = f;
}