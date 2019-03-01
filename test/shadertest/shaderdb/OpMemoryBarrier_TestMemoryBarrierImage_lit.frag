#version 450

layout(binding = 0, rgba32f) uniform image2D img2D;

void main()
{
    imageStore(img2D, ivec2(1, 2), vec4(0.1));
    memoryBarrierImage();
    imageStore(img2D, ivec2(2, 1), vec4(1.0));
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fence acq_rel

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
