#version 450 core

layout(set = 0, binding = 0) uniform isampler2D iSamp2D;
layout(set = 0, binding = 1) uniform usampler2D uSamp2D;
layout(location = 0) out ivec4 oColor1;
layout(location = 1) out uvec4 oColor2;

void main()
{
    const ivec2 temp[4] = {{1, 1}, {2, 2}, {3, 3}, {4, 4}};
    oColor1  = textureGather(iSamp2D, vec2(0, 1), 0);
    oColor1 += textureGatherOffset(iSamp2D, vec2(0, 1), ivec2(1, 2), 0);
    oColor1 += textureGatherOffsets(iSamp2D, vec2(0, 1), temp, 0);
    oColor2  = textureGather(uSamp2D, vec2(0, 1), 0);
    oColor2 += textureGatherOffset(uSamp2D, vec2(0, 1), ivec2(1, 2), 0);
    oColor2 += textureGatherOffsets(uSamp2D, vec2(0, 1), temp, 0);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
