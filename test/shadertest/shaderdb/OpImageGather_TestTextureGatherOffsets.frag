#version 450

layout(set = 0, binding = 0) uniform sampler2D      samp2D;
layout(set = 1, binding = 0) uniform sampler2DArray samp2DArray[4];
layout(set = 0, binding = 1) uniform sampler2DRect  samp2DRect;

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    const ivec2 i2[4] = { ivec2(1), ivec2(2), ivec2(3), ivec2(4) };

    vec4 f4 = textureGatherOffsets(samp2D, vec2(0.1), i2, 2);
    f4 += textureGatherOffsets(samp2DArray[index], vec3(0.2), i2, 3);
    f4 += textureGatherOffsets(samp2DRect, vec2(1.0), i2);

    fragColor = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
