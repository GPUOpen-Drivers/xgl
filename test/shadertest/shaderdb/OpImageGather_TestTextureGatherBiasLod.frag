#version 450 core

#extension GL_AMD_texture_gather_bias_lod: enable

layout(binding = 0) uniform sampler2D           s2D;
layout(binding = 1) uniform sampler2DArray      s2DArray;
layout(binding = 2) uniform samplerCube         sCube;
layout(binding = 3) uniform samplerCubeArray    sCubeArray;

layout(location = 1) in vec2 c2;
layout(location = 2) in vec3 c3;
layout(location = 3) in vec4 c4;

layout(location = 4) in float lod;
layout(location = 5) in float bias;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 texel  = vec4(0.0);
    vec4 result = vec4(0.0);

    const ivec2 offsets[4] = { ivec2(0, 0), ivec2(0, 1), ivec2(1, 0), ivec2(1, 1) };

    texel += textureGather(s2D,        c2, 0, bias);

    texel += textureGather(s2DArray,   c3, 1, bias);
    texel += textureGather(sCube,      c3, 2, bias);
    texel += textureGather(sCubeArray, c4, 3, bias);

    texel += textureGatherOffset(s2D,        c2, offsets[0], 0, bias);
    texel += textureGatherOffset(s2DArray,   c3, offsets[1], 1, bias);

    texel += textureGatherOffsets(s2D,        c2, offsets, 0, bias);
    texel += textureGatherOffsets(s2DArray,   c3, offsets, 1, bias);

    texel += textureGatherLodAMD(s2D,        c2, lod);
    texel += textureGatherLodAMD(s2DArray,   c3, lod, 1);
    texel += textureGatherLodAMD(sCube,      c3, lod, 2);
    texel += textureGatherLodAMD(sCubeArray, c4, lod, 3);

    texel += textureGatherLodOffsetAMD(s2D,        c2, lod, offsets[0]);
    texel += textureGatherLodOffsetAMD(s2DArray,   c3, lod, offsets[1], 1);

    texel += textureGatherLodOffsetsAMD(s2D,       c2, lod, offsets);
    texel += textureGatherLodOffsetsAMD(s2DArray,  c3, lod, offsets, 1);

    fragColor = texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
