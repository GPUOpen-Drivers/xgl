#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2_0;
    dmat2 dm2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dvec2 d2_1 = dm2 * d2_0;

    fragColor = (d2_0 == d2_1) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: <2 x double> @_Z17MatrixTimesVectorDv2_Dv2_dDv2_d

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <2 x double>
; SHADERTEST: fadd <2 x double>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
