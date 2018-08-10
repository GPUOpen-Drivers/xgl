#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 2) in vec4 c;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fmix = mix(a, b, c);
    ivec4 imix = mix(ivec4(a), ivec4(b), bvec4(c));
    uvec4 umix = mix(uvec4(a), uvec4(b), bvec4(c));
    frag_color = fmix + vec4(imix) + vec4(umix);
}