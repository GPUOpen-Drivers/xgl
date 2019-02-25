#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 0) out vec4 frag_color;

void main()
{
    // FrexpStruct
    ivec4 fo = ivec4(0);
    vec4 fv = frexp(a, fo);
    frag_color = vec4(fv * fo);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { <4 x float>, <4 x i32> } @_Z11frexpStructDv4_f(<4 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call float @llvm.amdgcn.frexp.mant.f32(float %{{.*}})
; SHADERTEST: %{{[0-9]*}} = {{.*}} call i32 @llvm.amdgcn.frexp.exp.i32.f32(float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
