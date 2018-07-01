#version 450 core

layout(std430, binding = 0) buffer Block
{
    float f1;
    vec2  f2;
    vec3  f3;
    vec4  f4;
} block;

void main()
{
    block.f1 += 1.0;
    block.f2 += vec2(2.0);
    block.f3 += vec3(3.0);
    block.f4 += vec4(4.0);

    gl_Position = vec4(1.0);
}