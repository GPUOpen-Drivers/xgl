#version 450

layout(set = 0, binding = 0, rgba32f) uniform image1D           img1D;
layout(set = 0, binding = 1, rgba32f) uniform image2DRect       img2DRect;
layout(set = 1, binding = 0, rgba32f) uniform imageBuffer       imgBuffer[4];
layout(set = 2, binding = 0, rgba32f) uniform imageCubeArray    imgCubeArray[4];
layout(set = 0, binding = 2, rgba32f) uniform image2DMS         img2DMS;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
    vec4 data;
};

void main()
{
    imageStore(img1D, 1, data);
    imageStore(img2DRect, ivec2(2, 3), data);
    imageStore(imgBuffer[index], 4, data);
    imageStore(imgCubeArray[index + 1], ivec3(5, 6, 7), data);
    imageStore(img2DMS, ivec2(8, 9), 3, data);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
