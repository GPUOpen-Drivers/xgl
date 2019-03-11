#version 450

layout(set = 0, binding = 0) uniform sampler1D  samp1D;
layout(set = 1, binding = 0) uniform sampler2D  samp2D[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = textureLodOffset(samp1D, 0.5, 0.4, 6);
    f4 += textureLodOffset(samp2D[index], vec2(0.6), 0.7, ivec2(5));

    fragColor = f4;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @spirv.image.sample.f32.1D.lod.constoffset({{.*}}, float 5.000000e-01, float 0x3FD99999A0000000, i32 6, {{.*}})
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D.lod.constoffset({{.*}}, <2 x float> <float 0x3FE3333340000000, float 0x3FE3333340000000>, float 0x3FE6666660000000, <2 x i32> <i32 5, i32 5>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.1D.lod.constoffset{{.*}}({{.*}}, float 5.000000e-01, float 0x3FD99999A0000000, i32 6, {{.*}})
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D.lod.constoffset{{.*}}({{.*}}, <2 x float> <float 0x3FE3333340000000, float 0x3FE3333340000000>, float 0x3FE6666660000000, <2 x i32> <i32 5, i32 5>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.l.o.1d.v4f32.f32({{.*}}, i32 6, float 5.000000e-01, float 0x3FD99999A0000000, {{.*}})
; SHADERTEST: call i32 @llvm.amdgcn.readfirstlane
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.l.o.2d.v4f32.f32({{.*}}, i32 1285, float 0x3FE3333340000000, float 0x3FE3333340000000, float 0x3FE6666660000000, {{.*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
