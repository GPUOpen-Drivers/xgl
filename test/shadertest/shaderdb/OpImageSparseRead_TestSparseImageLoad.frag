#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0, rgba32f) uniform image2D       img2D[4];
layout(set = 1, binding = 0, rgba32f) uniform image2DRect   img2DRect;
layout(set = 2, binding = 0, rgba32f) uniform image2DMS     img2DMS;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseImageLoadARB(img2D[index], ivec2(1), texel);
    fragColor += texel;

    sparseImageLoadARB(img2DRect, ivec2(2), texel);
    fragColor += texel;

    sparseImageLoadARB(img2DMS, ivec2(2), 3, texel);
    fragColor += texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
