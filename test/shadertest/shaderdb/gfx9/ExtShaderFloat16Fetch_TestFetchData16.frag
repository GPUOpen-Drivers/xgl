#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_half_float_fetch: enable

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

layout(location =  0) in float c1;
layout(location =  1) in vec2  c2;
layout(location =  2) in vec3  c3;
layout(location =  3) in vec4  c4;

layout(location =  4) in float lod;

layout(location = 0) out vec4 fragColor;

void main()
{
    f16vec4 texel = f16vec4(0.0hf);

    texel   += texelFetch(s1D, int(c1), int(lod));
    texel   += texelFetch(s2D, ivec2(c2), int(lod));
    texel   += texelFetch(s3D, ivec3(c3), int(lod));
    texel   += texelFetch(s2DRect, ivec2(c2));
    texel   += texelFetch(s1DArray, ivec2(c2), int(lod));
    texel   += texelFetch(s2DArray, ivec3(c3), int(lod));
    texel   += texelFetch(sBuffer, int(c1));
    texel   += texelFetch(s2DMS, ivec2(c2), 1);
    texel   += texelFetch(s2DMSArray, ivec3(c3), 2);

    fragColor = texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
