#version 450

layout(location = 0) in centroid float f1_1;
layout(location = 1) in flat sample vec4 f4_1;

layout(binding = 0) uniform Uniforms
{
    int i;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = interpolateAtSample(f1_1, i);

    vec4 f4_0 = interpolateAtSample(f4_1, i);

    fragColor = (f4_0.y == f1_0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} float @_Z19interpolateAtSamplePfi(float addrspace(64)* @f1_1, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z19interpolateAtSamplePDv4_fi(<4 x float> addrspace(64)* @f4_1, i32 %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call <2 x float> @llpc.input.interpolate.evalij.sample(i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call float @llpc.input.import.interpolant.f32.i32.i32.i32.i32.v2f32(i32 0, i32 0, i32 0, i32 0, <2 x float> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call <2 x float> @llpc.input.interpolate.evalij.sample(i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call <4 x float> @llpc.input.import.interpolant.v4f32.i32.i32.i32.i32.v2f32(i32 1, i32 0, i32 0, i32 1, <2 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.interp.p1(float %{{.*}}, i32 0, i32 0, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.interp.p2(float %{{.*}}, float %{{.*}}, i32 0, i32 0, i32 %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.interp.mov(i32 2, i32 1, i32 1, i32 %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
