#version 450 core

#define ITER 16
layout(set=0, binding=0) uniform UniformBuffer
{
    vec4 c[4];
    ivec4 ci[4];
    double cd[4];
};

struct TSTR {
    vec4 member;
};

layout(location = 0) out vec4 frag_color;
layout(location = 0) in TSTR interp[ITER];

void main()
{
    int i;
    vec4 s = vec4(0.0f);
    vec2 offset = vec2(0.00001);
    for (i =0; i < ITER; i++)
    {
        offset = vec2(float(i)/float(ITER));
        s += interpolateAtOffset(interp[i].member, offset);
    }
    frag_color.x = float(s.x - interp[0].member.x);
    frag_color.y = float(s.y + interp[0].member.y);
    frag_color.z = 0.0f;
    frag_color.w = 1.0f;
}

