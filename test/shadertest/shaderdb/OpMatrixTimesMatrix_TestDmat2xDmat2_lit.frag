#version 450

layout(binding = 0) uniform Uniforms
{
    dmat2 dm2_0;
    dmat2 dm2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);


    dmat2 dm2_2 = dm2_0 * dm2_1;

    fragColor = (dm2_2[0] == dm2_0[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [2 x <2 x double>] @_Z17MatrixTimesMatrixDv2_Dv2_dDv2_Dv2_d([2 x <2 x double>] %{{[0-9]*}}, [2 x <2 x double>] %{{[0-9]*}})

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <2 x double> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd <2 x double> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
