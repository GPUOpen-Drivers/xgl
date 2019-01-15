#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_half_float_fetch: enable

layout(set = 1, binding =  0) layout(rgba16f) uniform f16image1D          i1D;
layout(set = 1, binding =  1) layout(rgba16f) uniform f16image2D          i2D;
layout(set = 1, binding =  2) layout(rgba16f) uniform f16image3D          i3D;
layout(set = 1, binding =  3) layout(rgba16f) uniform f16image2DRect      i2DRect;
layout(set = 1, binding =  4) layout(rgba16f) uniform f16imageCube        iCube;
layout(set = 1, binding =  5) layout(rgba16f) uniform f16image1DArray     i1DArray;
layout(set = 1, binding =  6) layout(rgba16f) uniform f16image2DArray     i2DArray;
layout(set = 1, binding =  7) layout(rgba16f) uniform f16imageCubeArray   iCubeArray;
layout(set = 1, binding =  8) layout(rgba16f) uniform f16imageBuffer      iBuffer;
layout(set = 1, binding =  9) layout(rgba16f) uniform f16image2DMS        i2DMS;
layout(set = 1, binding = 10) layout(rgba16f) uniform f16image2DMSArray   i2DMSArray;

layout(location =  0) in float c1;
layout(location =  1) in vec2  c2;
layout(location =  2) in vec3  c3;
layout(location =  3) in vec4  c4;

layout(location = 0) out vec4 fragColor;

void main()
{
    f16vec4 texel = f16vec4(0.0hf);

    texel += imageLoad(i1D, int(c1));
    texel += imageLoad(i2D, ivec2(c2));
    texel += imageLoad(i3D, ivec3(c3));
    texel += imageLoad(i2DRect, ivec2(c2));
    texel += imageLoad(iCube, ivec3(c3));
    texel += imageLoad(iBuffer, int(c1));
    texel += imageLoad(i1DArray, ivec2(c2));
    texel += imageLoad(i2DArray, ivec3(c3));
    texel += imageLoad(iCubeArray, ivec3(c3));
    texel += imageLoad(i2DMS, ivec2(c2), 1);
    texel += imageLoad(i2DMSArray, ivec3(c3), 1);

    f16vec4 data = texel;
    imageStore(i1D, int(c1), data);
    imageStore(i2D, ivec2(c2), data);
    imageStore(i3D, ivec3(c3), data);
    imageStore(i2DRect, ivec2(c2), data);
    imageStore(iCube, ivec3(c3), data);
    imageStore(iBuffer, int(c1), data);
    imageStore(i1DArray, ivec2(c2), data);
    imageStore(i2DArray, ivec3(c3), data);
    imageStore(iCubeArray, ivec3(c3), data);
    imageStore(i2DMS, ivec2(c2), 1, data);
    imageStore(i2DMSArray, ivec3(c3), 1, data);

    fragColor = texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
