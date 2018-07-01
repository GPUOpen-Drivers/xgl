#version 450 core

layout(set = 0, binding = 0) uniform isampler2D iSamp2D;
layout(set = 0, binding = 1) uniform usampler2D uSamp2D;
layout(location = 0) out ivec4 oColor1;
layout(location = 1) out uvec4 oColor2;

void main()
{
    oColor1 = texelFetch(iSamp2D, ivec2(0, 1), 0);
    oColor2 = texelFetch(uSamp2D, ivec2(0, 1), 0);
}
