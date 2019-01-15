#version 450

layout(set = 0, binding = 0) uniform sampler2DRect       samp2DRect;
layout(set = 1, binding = 0) uniform samplerBuffer       samp2DBuffer[4];
layout(set = 0, binding = 1) uniform sampler2DMS         samp2DMS;
layout(set = 2, binding = 0) uniform sampler2DMSArray    samp2DMSArray[4];

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out ivec3 i3;

void main()
{
    i3.xy  = textureSize(samp2DRect);
    i3.x  += textureSize(samp2DBuffer[index]);
    i3.xy += textureSize(samp2DMS);
    i3    += textureSize(samp2DMSArray[index]);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
