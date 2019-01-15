#version 450

layout(set = 0, binding = 0) uniform sampler2DMS          samp2DMS;
layout(set = 0, binding = 1) uniform sampler2DMSArray     samp2DMSArray[4];

layout(set = 1, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1 = textureSamples(samp2DMS);
    i1 += textureSamples(samp2DMSArray[index]);

    fragColor = vec4(i1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
