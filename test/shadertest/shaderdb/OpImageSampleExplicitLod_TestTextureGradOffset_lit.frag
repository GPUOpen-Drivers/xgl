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
    vec4 f4 = textureGradOffset(samp1D, 1.0, 2.0, 3.0, 2);
    f4 += textureGradOffset(samp2D[index], vec2(4.0), vec2(5.0), vec2(6.0), ivec2(3));

    fragColor = f4;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @spirv.image.sample.f32.1D.grad.constoffset({{.*}}, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, i32 2, {{.*}})
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D.grad.constoffset({{.*}}, <2 x float> <float 4.000000e+00, float 4.000000e+00>, <2 x float> <float 5.000000e+00, float 5.000000e+00>, <2 x float> <float 6.000000e+00, float 6.000000e+00>, <2 x i32> <i32 3, i32 3>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.1D.grad.constoffset{{.*}}({{.*}}, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, i32 2, {{.*}})
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D.grad.constoffset{{.*}}({{.*}}, <2 x float> <float 4.000000e+00, float 4.000000e+00>, <2 x float> <float 5.000000e+00, float 5.000000e+00>, <2 x float> <float 6.000000e+00, float 6.000000e+00>, <2 x i32> <i32 3, i32 3>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.o.1d.v4f32.f32.f32({{.*}}, i32 2, float 2.000000e+00, float 3.000000e+00, float 1.000000e+00, {{.*}})
; SHADERTEST: call i32 @llvm.amdgcn.readfirstlane
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.o.2d.v4f32.f32.f32({{.*}}, i32 771, float 5.000000e+00, float 5.000000e+00, float 6.000000e+00, float 6.000000e+00, float 4.000000e+00, float 4.000000e+00, {{.*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
