#version 450

layout(binding = 0) uniform Uniforms
{
    mat2 m2;
    mat3 m3;
    mat4 m4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1 = determinant(m2);

    f1 += determinant(m3);

    f1 += determinant(m4);

    fragColor = (f1 > 0.0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
