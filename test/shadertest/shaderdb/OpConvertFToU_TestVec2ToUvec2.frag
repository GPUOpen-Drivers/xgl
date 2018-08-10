#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec2 u2 = uvec2(f2);

    fragColor = (u2.x == u2.y) ? vec4(0.0) : vec4(1.0);
}