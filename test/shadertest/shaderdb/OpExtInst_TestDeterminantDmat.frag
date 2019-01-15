#version 450

layout(binding = 0) uniform Uniforms
{
    dmat2 dm2;
    dmat3 dm3;
    dmat4 dm4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    double d1 = determinant(dm2);

    d1 += determinant(dm3);

    d1 += determinant(dm4);

    fragColor = (d1 > 0.0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
