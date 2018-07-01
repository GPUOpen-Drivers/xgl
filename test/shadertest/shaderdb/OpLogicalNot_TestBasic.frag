
#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    vec4 nf = vec4(0.0f);
    bvec4 bd = bvec4(colorIn1);
    nf += -colorIn1;
    uvec4 nuvf = - uvec4(colorIn1);
    nuvf.x = ~nuvf.x;
    ivec4 iuvf = - ivec4(colorIn2);
    bd = not(bd);
    bool bc = !bd.x || false;
    bc = bc && true;
    bd.z = bd.z && bc;
    color = vec4(bd)  + nf + vec4(nuvf) + vec4(iuvf);
}