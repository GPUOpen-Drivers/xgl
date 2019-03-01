#version 450

layout(binding = 0) uniform Uniforms
{
    mat4 m4_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat4 m4_1 = transpose(m4_0);

    fragColor = (m4_0[2] == m4_1[3]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [4 x <4 x float>] @_Z9TransposeDv4_Dv4_f

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
