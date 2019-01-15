#version 450

layout(binding = 0) uniform Uniforms
{
    double d1;
    dvec4  d4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec4 d4_0 = d1 * d4;
    fragColor = vec4(d4_0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
