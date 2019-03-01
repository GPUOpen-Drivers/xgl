#version 450 core

layout(location = 0) in vec4 fIn;
layout(location = 0, xfb_buffer = 1, xfb_offset = 24) out vec3 fOut1;
layout(location = 1, xfb_buffer = 0, xfb_offset = 16) out vec2 fOut2;

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
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v2f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
