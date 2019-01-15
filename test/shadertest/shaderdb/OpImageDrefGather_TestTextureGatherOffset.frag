#version 450

layout(set = 0, binding = 0) uniform sampler2DShadow      samp2DShadow;
layout(set = 1, binding = 0) uniform sampler2DArrayShadow samp2DArrayShadow[4];
layout(set = 0, binding = 1) uniform sampler2DRectShadow  samp2DRectShadow;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
    ivec2 i2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = textureGatherOffset(samp2DShadow, vec2(0.1), 0.9, ivec2(1));
    f4 += textureGatherOffset(samp2DArrayShadow[index], vec3(0.2), 0.8, i2);
    f4 += textureGatherOffset(samp2DRectShadow, vec2(1.0), 0.7, i2);

    fragColor = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
