#version 450

struct S
{
    vec4 f4;
    mat4 m4;
};

layout(location = 1) noperspective sample in S s;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = s.f4;
    f += s.m4[1];

    fragColor = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-5: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: call float @llvm.amdgcn.interp.p1
; SHADERTEST: call float @llvm.amdgcn.interp.p2
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
