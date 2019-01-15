#version 450

layout(set = 0, binding = 0) uniform sampler1D samp1D;
layout(set = 1, binding = 0) uniform sampler2D samp2D[4];
layout(set = 0, binding = 1) uniform sampler2DRect samp2DRect;
layout(set = 0, binding = 2) uniform samplerBuffer sampBuffer;
layout(set = 0, binding = 3) uniform sampler2DMS samp2DMS[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = texelFetch(samp1D, 2, 2);
    f4 += texelFetch(samp2D[index], ivec2(7), 8);
    f4 += texelFetch(samp2DRect, ivec2(3));
    f4 += texelFetch(sampBuffer, 5);
    f4 += texelFetch(samp2DMS[index], ivec2(6), 4);

    fragColor = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
