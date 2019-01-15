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
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
