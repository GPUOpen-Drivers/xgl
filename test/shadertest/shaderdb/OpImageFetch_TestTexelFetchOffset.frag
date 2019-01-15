#version 450

layout(set = 0, binding = 0) uniform sampler1D samp1D;
layout(set = 1, binding = 0) uniform sampler2D samp2D[4];
layout(set = 0, binding = 1) uniform sampler3D samp3D;
layout(set = 0, binding = 2) uniform sampler2DRect samp2DRect;
layout(set = 0, binding = 3) uniform sampler1DArray samp1DArray;
layout(set = 2, binding = 0) uniform sampler2DArray samp2DArray[4];

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = texelFetchOffset(samp1D, 2, 3, 4);
    f4 += texelFetchOffset(samp2D[index], ivec2(5), 6, ivec2(7));
    f4 += texelFetchOffset(samp3D, ivec3(1), 2, ivec3(3));
    f4 += texelFetchOffset(samp2DRect, ivec2(4), ivec2(5));
    f4 += texelFetchOffset(samp1DArray, ivec2(5), 6, 7);
    f4 += texelFetchOffset(samp2DArray[index], ivec3(1), 2, ivec2(3));

    fragColor = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
