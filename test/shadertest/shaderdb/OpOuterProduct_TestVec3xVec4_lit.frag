#version 450 core

layout(location = 0) in vec3 l0 ;
layout(location = 1) in vec4 l1 ;
layout(location = 0) out vec4 color;


void main()
{
    mat4x3 x = outerProduct(l0,l1);
    color.xyz = x[0] + x[1] + x[2] + x[3];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [4 x <3 x float>] @_Z12OuterProductDv3_fDv4_f

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
