#version 450 core

#extension GL_AMD_shader_fragment_mask: enable

layout(binding = 0) uniform sampler2DMS       s2DMS;
layout(binding = 1) uniform isampler2DMSArray is2DMSArray;

layout(binding = 2, input_attachment_index = 0) uniform usubpassInputMS usubpassMS;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = vec4(0.0);

    uint fragMask = fragmentMaskFetchAMD(s2DMS, ivec2(2, 3));
    uint fragIndex = (fragMask & 0xF0) >> 4;
    f4 += fragmentFetchAMD(s2DMS, ivec2(2, 3), fragIndex);

    fragMask = fragmentMaskFetchAMD(is2DMSArray, ivec3(2, 3, 1));
    fragIndex = (fragMask & 0xF0) >> 4;
    f4 += fragmentFetchAMD(is2DMSArray, ivec3(2, 3, 1), fragIndex);

    fragMask = fragmentMaskFetchAMD(usubpassMS);
    fragIndex = (fragMask & 0xF0) >> 4;
    f4 += fragmentFetchAMD(usubpassMS, fragIndex);

    fragColor = f4;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.image.fetch.u32.2D.fmaskvalue
; SHADERTEST: call <4 x float> @llpc.image.fetch.f32.2D.sample
; SHADERTEST: call i32 @llpc.image.fetch.u32.2DArray.fmaskvalue
; SHADERTEST: call <4 x i32> @llpc.image.fetch.i32.2DArray.sample
; SHADERTEST: call i32 @llpc.image.fetch.u32.SubpassData.fmaskvalue
; SHADERTEST: call <4 x i32> @llpc.image.fetch.u32.SubpassData.sample
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: tail call float @llvm.amdgcn.image.load.2d.f32.i32
; SHADERTEST: tail call <4 x float> @llvm.amdgcn.image.load.2dmsaa.v4f32.i32
; SHADERTEST: tail call float @llvm.amdgcn.image.load.3d.f32.i32
; SHADERTEST: tail call <4 x float> @llvm.amdgcn.image.load.2darraymsaa.v4f32.i32
; SHADERTEST: tail call float @llvm.amdgcn.image.load.2d.f32.i32
; SHADERTEST: tail call <4 x float> @llvm.amdgcn.image.load.2dmsaa.v4f32.i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
