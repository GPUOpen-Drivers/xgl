#version 450 core

layout(location = 0) in dvec4 fIn;
layout(location = 0, xfb_buffer = 1, xfb_offset = 24) out dvec3 fOut1;
layout(location = 2, xfb_buffer = 0, xfb_offset = 16) out dvec2 fOut2;

void main()
{
    fOut1 = fIn.xyz;
    fOut2 = fIn.xy;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v3f64
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f64
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v2f64
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f64
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
