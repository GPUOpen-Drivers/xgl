#version 450

layout(location = 1) in Block
{
    flat int i1;
    centroid vec4 f4;
    noperspective sample mat4 m4;
} block;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f = block.f4;
    f += block.m4[1];
    f += vec4(block.i1);

    fragColor = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.input.import.generic{{.*}}
; SHADERTEST-COUNT-5: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call float @llvm.amdgcn.interp.mov
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
