#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    bvec4 x = greaterThan (colorIn1, colorIn2);
    bvec4 y = greaterThan (uvec4(colorIn1), uvec4(colorIn2));
    bvec4 z = greaterThan (ivec4(colorIn1), ivec4(colorIn2));

    bvec4 w = equal(x,y);
    bvec4 q = notEqual(w,z);
    vec4 color = vec4(q);
}