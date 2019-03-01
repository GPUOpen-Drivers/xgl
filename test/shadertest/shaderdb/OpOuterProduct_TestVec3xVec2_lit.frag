#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
    vec3 f3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    mat2x3 m2x3 = outerProduct(f3, f2);

    fragColor = (m2x3[0] != m2x3[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [2 x <3 x float>] @_Z12OuterProductDv3_fDv2_f

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
