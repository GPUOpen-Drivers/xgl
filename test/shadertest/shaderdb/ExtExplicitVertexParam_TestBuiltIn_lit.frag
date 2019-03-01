#version 450 core

#extension GL_AMD_shader_explicit_vertex_parameter: enable

layout(location = 0) out vec2 fOut;

void main()
{
    fOut = gl_BaryCoordNoPerspAMD;
    fOut += gl_BaryCoordNoPerspCentroidAMD;
    fOut += gl_BaryCoordNoPerspSampleAMD;
    fOut += gl_BaryCoordSmoothAMD;
    fOut += gl_BaryCoordSmoothCentroidAMD;
    fOut += gl_BaryCoordSmoothSampleAMD;
    fOut += gl_BaryCoordPullModelAMD.xy + gl_BaryCoordPullModelAMD.yz;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <3 x float> @llpc.input.import.builtin.BaryCoordPullModelAMD.v3f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordSmoothSampleAMD.v2f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordSmoothCentroidAMD.v2f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordSmoothAMD.v2f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordNoPerspSampleAMD.v2f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordNoPerspCentroidAMD.v2f32.i32
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.BaryCoordNoPerspAMD.v2f32.i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
