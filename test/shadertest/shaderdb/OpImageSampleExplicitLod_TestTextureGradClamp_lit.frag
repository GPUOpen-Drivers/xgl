#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler2D          samp2D[4];
layout(set = 1, binding = 0) uniform sampler3D          samp3D;

layout(set = 2, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 texel = vec4(0.0);
    vec4 f4 = vec4(0.0);

    texel = textureGradClampARB(samp2D[index], vec2(0.1), vec2(1.0), vec2(1.1), lodClamp);
    f4 += texel;

    texel = textureGradClampARB(samp3D, vec3(0.2), vec3(1.2), vec3(1.3), lodClamp);
    f4 += texel;

    texel = textureGradOffsetClampARB(samp2D[index], vec2(0.1), vec2(1.0), vec2(1.1), ivec2(2), lodClamp);
    f4 += texel;

    texel = textureGradOffsetClampARB(samp3D, vec3(0.2), vec3(1.2), vec3(1.3), ivec3(3), lodClamp);
    f4 += texel;

    fragColor = f4;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D.grad.minlod({{.*}}, <2 x float> <float 0x3FB99999A0000000, float 0x3FB99999A0000000>, <2 x float> <float 1.000000e+00, float 1.000000e+00>, <2 x float> <float 0x3FF19999A0000000, float 0x3FF19999A0000000>, {{.*}})
; SHADERTEST: <4 x float> @spirv.image.sample.f32.3D.grad.minlod({{.*}}, <3 x float> <float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000>, <3 x float> <float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000>, <3 x float> <float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000>, {{.*}})
; SHADERTEST: <4 x float> @spirv.image.sample.f32.2D.grad.constoffset.minlod({{.*}}, <2 x float> <float 0x3FB99999A0000000, float 0x3FB99999A0000000>, <2 x float> <float 1.000000e+00, float 1.000000e+00>, <2 x float> <float 0x3FF19999A0000000, float 0x3FF19999A0000000>, <2 x i32> <i32 2, i32 2>, {{.*}})
; SHADERTEST: <4 x float> @spirv.image.sample.f32.3D.grad.constoffset.minlod({{.*}}, <3 x float> <float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000>, <3 x float> <float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000>, <3 x float> <float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000>, <3 x i32> <i32 3, i32 3, i32 3>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D.grad.minlod{{.*}}({{.*}}, <2 x float> <float 0x3FB99999A0000000, float 0x3FB99999A0000000>, <2 x float> <float 1.000000e+00, float 1.000000e+00>, <2 x float> <float 0x3FF19999A0000000, float 0x3FF19999A0000000>, {{.*}})
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.3D.grad.minlod{{.*}}({{.*}}, <3 x float> <float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000>, <3 x float> <float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000>, <3 x float> <float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000>, {{.*}})
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.2D.grad.constoffset.minlod{{.*}}({{.*}}, <2 x float> <float 0x3FB99999A0000000, float 0x3FB99999A0000000>, <2 x float> <float 1.000000e+00, float 1.000000e+00>, <2 x float> <float 0x3FF19999A0000000, float 0x3FF19999A0000000>, <2 x i32> <i32 2, i32 2>, {{.*}})
; SHADERTEST: call <4 x float> @llpc.image.sample.f32.3D.grad.constoffset.minlod{{.*}}({{.*}}, <3 x float> <float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000>, <3 x float> <float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000>, <3 x float> <float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000>, <3 x i32> <i32 3, i32 3, i32 3>, {{.*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i32 @llvm.amdgcn.readfirstlane
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.cl.2d.v4f32.f32.f32({{.*}}, float 1.000000e+00, float 1.000000e+00, float 0x3FF19999A0000000, float 0x3FF19999A0000000, float 0x3FB99999A0000000, float 0x3FB99999A0000000, {{.*}})
; SHADERTEST: load <4 x i32>, <4 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: load <8 x i32>, <8 x i32> addrspace(4)* %{{[0-9]*}}
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.cl.3d.v4f32.f32.f32({{.*}}, float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000, {{.*}})
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.cl.o.2d.v4f32.f32.f32({{.*}}, i32 514, float 1.000000e+00, float 1.000000e+00, float 0x3FF19999A0000000, float 0x3FF19999A0000000, float 0x3FB99999A0000000, float 0x3FB99999A0000000, {{.*}})
; SHADERTEST: call <4 x float> @llvm.amdgcn.image.sample.d.cl.o.3d.v4f32.f32.f32({{.*}}, i32 197379, float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF3333340000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FF4CCCCC0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000, {{.*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
