#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_half_float_fetch: enable
#extension GL_AMD_texture_gather_bias_lod: enable

layout(set = 0, binding =  0) uniform f16sampler1D            s1D;
layout(set = 0, binding =  1) uniform f16sampler2D            s2D;
layout(set = 0, binding =  2) uniform f16sampler3D            s3D;
layout(set = 0, binding =  3) uniform f16sampler2DRect        s2DRect;
layout(set = 0, binding =  4) uniform f16samplerCube          sCube;
layout(set = 0, binding =  5) uniform f16samplerBuffer        sBuffer;
layout(set = 0, binding =  6) uniform f16sampler2DMS          s2DMS;
layout(set = 0, binding =  7) uniform f16sampler1DArray       s1DArray;
layout(set = 0, binding =  8) uniform f16sampler2DArray       s2DArray;
layout(set = 0, binding =  9) uniform f16samplerCubeArray     sCubeArray;
layout(set = 0, binding = 10) uniform f16sampler2DMSArray     s2DMSArray;

layout(set = 0, binding = 11) uniform f16sampler1DShadow          s1DShadow;
layout(set = 0, binding = 12) uniform f16sampler2DShadow          s2DShadow;
layout(set = 0, binding = 13) uniform f16sampler2DRectShadow      s2DRectShadow;
layout(set = 0, binding = 14) uniform f16samplerCubeShadow        sCubeShadow;
layout(set = 0, binding = 15) uniform f16sampler1DArrayShadow     s1DArrayShadow;
layout(set = 0, binding = 16) uniform f16sampler2DArrayShadow     s2DArrayShadow;
layout(set = 0, binding = 17) uniform f16samplerCubeArrayShadow   sCubeArrayShadow;

layout(location =  0) in float c1;
layout(location =  1) in vec2  c2;
layout(location =  2) in vec3  c3;
layout(location =  3) in vec4  c4;

layout(location =  4) in float compare;

const ivec2 offsets[4] = { ivec2(0), ivec2(1), ivec2(2), ivec2(3),};

layout(location = 0) out vec4 fragColor;

void main()
{
    f16vec4 texel = f16vec4(0.0hf);

    texel   += textureGather(s2D, c2, 0);
    texel   += textureGather(s2DArray, c3, 0);
    texel   += textureGather(sCube, c3, 0);
    texel   += textureGather(sCubeArray, c4, 0);
    texel   += textureGather(s2DRect, c2, 0);
    texel   += textureGather(s2DShadow, c2, compare);
    texel   += textureGather(s2DArrayShadow, c3, compare);
    texel   += textureGather(sCubeShadow, c3, compare);
    texel   += textureGather(sCubeArrayShadow, c4, compare);
    texel   += textureGather(s2DRectShadow, c2, compare);

    texel   += textureGatherOffsets(s2D, c2, offsets, 0);
    texel   += textureGatherOffsets(s2DArray, c3, offsets, 0);
    texel   += textureGatherOffsets(s2DRect, c2, offsets, 0);
    texel   += textureGatherOffsets(s2DShadow, c2, compare, offsets);
    texel   += textureGatherOffsets(s2DArrayShadow, c3, compare, offsets);
    texel   += textureGatherOffsets(s2DRectShadow, c2, compare, offsets);

    fragColor = texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
