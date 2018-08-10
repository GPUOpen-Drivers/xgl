#version 450 core

layout(vertices = 5) out;

struct S
{
    int     x;
    vec4    y;
    float   z[2];
};

layout(location = 2) out TheBlock
{
    S     blockS;
    float blockFa[3];
    S     blockSa[2];
    float blockF;
} tcBlock[];

void main(void)
{
    S block = { 1, vec4(2), { 0.5, 0.7} };
    tcBlock[gl_InvocationID].blockSa[1] = block;

    int i = gl_InvocationID;
    tcBlock[gl_InvocationID].blockSa[i].z[i + 1] = 5.0;

    gl_TessLevelInner[0] = 2.0;
    gl_TessLevelOuter[0] = 4.0;
}