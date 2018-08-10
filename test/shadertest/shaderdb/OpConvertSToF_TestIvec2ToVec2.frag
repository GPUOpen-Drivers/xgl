#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 f2 = vec2(i2);

    fragColor = (f2.x == f2.y) ? vec4(0.0) : vec4(1.0);
}