#version 450 core

layout(triangles) in;

layout(location = 1) out vec3 outData1;
layout(location = 2) out dvec4 outData2;

void main()
{
    outData1 = gl_in[2].gl_Position.xyz;
    outData2 = dvec4(gl_in[1].gl_PointSize);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v4f64
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
