#version 450

struct DATA
{
    vec3  f3[3];
    mat3  m3;
    bool  b1;
    ivec2 i2;
};

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const DATA info = { { vec3(0.1), vec3(0.5), vec3(1.0) }, mat3(1.0), true, ivec2(5) };

    DATA data = info;

    fragColor = data.b1 ? vec4(data.f3[i].y) : vec4(data.m3[i].z);
}

