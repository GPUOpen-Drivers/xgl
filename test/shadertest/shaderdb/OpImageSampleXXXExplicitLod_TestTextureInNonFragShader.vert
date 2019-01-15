#version 450

layout(binding = 0) uniform sampler1D       samp1D;
layout(binding = 1) uniform sampler1DShadow samp1DShadow;

void main()
{
    vec4 texel = vec4(0.0);
    texel += texture(samp1D, 0.5);
    texel += texture(samp1DShadow, vec3(0.1));
    texel += textureProj(samp1D, vec2(0.2));
    texel += textureProj(samp1DShadow, vec4(0.3));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
