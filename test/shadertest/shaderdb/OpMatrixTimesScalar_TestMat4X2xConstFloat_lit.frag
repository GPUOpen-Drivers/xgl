#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat4x2 m4x2 = mat4x2(1.0);
    m4x2 *= 0.5;

    fragColor = vec4(m4x2[0] + m4x2[1]  +  m4x2[2]  + m4x2[3], 0, 0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [4 x <2 x float>] @_Z17MatrixTimesScalarDv4_Dv2_ff

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32(i32 0, i32 15, float 1.000000e+00, float 5.000000e-01, float 0.000000e+00, float 0.000000e+00, i1 true, i1 true)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
