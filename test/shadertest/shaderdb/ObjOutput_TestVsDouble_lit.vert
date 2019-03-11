#version 450 core

layout(location = 1) out dvec4 d4;
layout(location = 3) out dvec3 d3[2];
layout(location = 8) out dmat2 dm2;

layout(binding = 0) uniform Uniforms
{
    int i;
};

void main()
{
    d4 = dvec4(1.0);
    d3[i] = dvec3(2.0);
    dm2 = dmat2(dvec2(0.5), dvec2(1.5));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v4f64
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f64
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
