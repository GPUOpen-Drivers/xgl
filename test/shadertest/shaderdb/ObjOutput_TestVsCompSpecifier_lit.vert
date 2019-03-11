#version 450 core

layout(location = 0, component = 0) out float f1;
layout(location = 0, component = 2) out vec2 f2;

void main()
{
    f1 = 2.0;
    f2 = vec2(4.0, 5.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32(i32 0, i32 0, float 2.000000e+00)
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32(i32 0, i32 2, <2 x float> <float 4.000000e+00, float 5.000000e+00>)
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
