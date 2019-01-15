#version 450

layout(binding = 0) uniform Uniforms
{
    vec3 f3;
    dvec2 d2_0, d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    mat2x3 m2x3 = mat2x3(f3, f3);

    dmat4x2 dm4x2 = dmat4x2(d2_0, d2_1, d2_0, d2_0);

    fragColor = ((m2x3[0] == m2x3[1]) && (dm4x2[0] == dm4x2[1])) ? vec4(1.0) : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
