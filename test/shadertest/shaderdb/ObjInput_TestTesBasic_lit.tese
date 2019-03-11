#version 450 core

layout(triangles) in;

layout(location = 1) in vec4 inData1[];
layout(location = 2) patch in dvec4 inData2[4];

void main()
{
    gl_Position = inData1[1] + vec4(inData2[gl_PrimitiveID].z);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x float> @llpc.input.import.generic.v4f32{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST: call double @llpc.input.import.generic.f64{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
