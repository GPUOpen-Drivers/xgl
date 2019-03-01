#version 450 core

#extension GL_AMD_shader_explicit_vertex_parameter: enable

layout(location = 0) in __explicitInterpAMD vec2 fv2In;
layout(location = 1) in __explicitInterpAMD ivec2 iv2In;
layout(location = 2) in __explicitInterpAMD uvec2 uv2In;

layout(location = 0) out vec2 fOut;

void main()
{
    fOut  = interpolateAtVertexAMD(fv2In, 2);
    fOut += vec2(interpolateAtVertexAMD(iv2In, 1));
    fOut += vec2(interpolateAtVertexAMD(uv2In, 0));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} <2 x float> @_Z22InterpolateAtVertexAMDPDv2_fi
; SHADERTEST: call {{.*}} <2 x i32> @_Z22InterpolateAtVertexAMDPDv2_ii
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <2 x float> @llpc.input.import.interpolant.v2f32{{.*}}
; SHADERTEST: call <2 x i32> @llpc.input.import.interpolant.v2i32{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
