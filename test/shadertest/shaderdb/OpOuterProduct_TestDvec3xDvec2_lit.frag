#version 450

layout(binding = 0) uniform Uniforms
{
    dvec2 d2;
    dvec3 d3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dmat2x3 dm2x3 = outerProduct(d3, d2);

    fragColor = (dm2x3[0][0] > 0.0) ? vec4(1.0) : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [2 x <3 x double>] @_Z12OuterProductDv3_dDv2_d

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
