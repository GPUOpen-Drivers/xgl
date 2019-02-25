#version 450

layout(binding = 0) uniform Uniforms
{
    mat2x3 m2x3;
    mat4x2 m4x2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat4x3 m4x3 = m2x3 * m4x2;

    fragColor = (m4x3[0] == m4x3[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [4 x <3 x float>] @_Z17MatrixTimesMatrixDv2_Dv3_fDv4_Dv2_f

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <3 x float> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: fadd <3 x float> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
