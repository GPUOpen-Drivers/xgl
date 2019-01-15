#version 450 core

#extension GL_ARB_sparse_texture2: enable
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

    sparseTextureGatherARB(s2D,        c2, result, 0, bias);
    texel += result;
    sparseTextureGatherARB(s2DArray,   c3, result, 1, bias);
    texel += result;
    sparseTextureGatherARB(sCube,      c3, result, 2, bias);
    texel += result;
    sparseTextureGatherARB(sCubeArray, c4, result, 2, bias);
    texel += result;

    sparseTextureGatherOffsetARB(s2D,      c2, offsets[0], result, 0, bias);
    texel += result;
    sparseTextureGatherOffsetARB(s2DArray, c3, offsets[1], result, 1, bias);
    texel += result;

    sparseTextureGatherOffsetsARB(s2D,      c2, offsets, result, 0, bias);
    texel += result;
    sparseTextureGatherOffsetsARB(s2DArray, c3, offsets, result, 1, bias);
    texel += result;

    sparseTextureGatherLodAMD(s2D,        c2, lod, result);
    texel += result;
    sparseTextureGatherLodAMD(s2DArray,   c3, lod, result, 1);
    texel += result;
    sparseTextureGatherLodAMD(sCube,      c3, lod, result, 2);
    texel += result;
    sparseTextureGatherLodAMD(sCubeArray, c4, lod, result, 2);
    texel += result;

    sparseTextureGatherLodOffsetAMD(s2D,      c2, lod, offsets[0], result);
    texel += result;
    sparseTextureGatherLodOffsetAMD(s2DArray, c3, lod, offsets[1], result, 1);
    texel += result;

    sparseTextureGatherLodOffsetsAMD(s2D,      c2, lod, offsets, result);
    texel += result;
    sparseTextureGatherLodOffsetsAMD(s2DArray, c3, lod, offsets, result, 1);
    texel += result;

    fragColor = texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
