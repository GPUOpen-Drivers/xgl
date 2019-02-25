#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
    dmat3 dm3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dmat3 dm4 = dm3 * d1;

    fragColor = (dm4[0] == dm4[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call spir_func [3 x <3 x double>] @_Z17MatrixTimesScalarDv3_Dv3_dd

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul double %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
