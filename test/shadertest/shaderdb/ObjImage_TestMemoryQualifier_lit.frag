#version 450

layout(set = 0, binding = 0, rgba32f) coherent readonly uniform image1D img1D;
layout(set = 0, binding = 1, rgba32f) restrict uniform image2D img2D;
layout(set = 0, binding = 2, rgba32f) volatile writeonly uniform image2DRect img2DRect;

void main()
{
    vec4 texel = vec4(0.0);
    texel += imageLoad(img1D, 1);
    texel += imageLoad(img2D, ivec2(2));

    imageStore(img2DRect, ivec2(3), texel);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} <4 x float> @spirv.image.read.f32.1D
; SHADERTEST: call {{.*}} <4 x float> @spirv.image.read.f32.2D
; SHADERTEST: call {{.*}} void @spirv.image.write.f32.Rect
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.read.f32.1D.dimaware
; SHADERTEST: call <4 x float> @llpc.image.read.f32.2D.dimaware
; SHADERTEST: call void @llpc.image.write.f32.Rect.dimaware
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
