#version 450

layout(binding = 0) uniform Uniforms
{
    dmat4x3 dm4x3;
    dmat3x4 dm3x4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dmat3 dm3 = dm4x3 * dm3x4;

    fragColor = (dm3[0] == dm3[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [3 x <3 x double>] @_Z17MatrixTimesMatrixDv4_Dv3_dDv3_Dv4_d

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <3 x double> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd <3 x double> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
