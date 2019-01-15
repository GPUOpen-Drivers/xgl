#version 450

layout(set = 0, binding = 0, rgba32f) uniform image1D           img1D;
layout(set = 0, binding = 1, rgba32f) uniform image2DRect       img2DRect;
layout(set = 1, binding = 0, rgba32f) uniform imageBuffer       imgBuffer[4];
layout(set = 2, binding = 0, rgba32f) uniform imageCubeArray    imgCubeArray[4];
layout(set = 0, binding = 2, rgba32f) uniform image2DMS         img2DMS;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = vec4(0.0);
    f4 += imageLoad(img1D, 1);
    f4 += imageLoad(img2DRect, ivec2(2, 3));
    f4 += imageLoad(imgBuffer[index], 4);
    f4 += imageLoad(imgCubeArray[index + 1], ivec3(5, 6, 7));
    f4 += imageLoad(img2DMS, ivec2(8, 9), 2);

    fragColor = f4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
