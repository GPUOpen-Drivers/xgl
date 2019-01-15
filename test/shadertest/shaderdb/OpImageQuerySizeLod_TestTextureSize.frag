#version 450

layout(set = 0, binding = 0) uniform sampler2D            samp2D[4];
layout(set = 0, binding = 4) uniform sampler2DShadow      samp2DShadow;
layout(set = 0, binding = 5) uniform sampler3D            samp3D;

layout(set = 1, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 2) out ivec3 i3;

void main()
{
    i3.xy  = textureSize(samp2D[index], 3);
    i3.xy += textureSize(samp2DShadow, 4);
    i3    += textureSize(samp3D, 5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
