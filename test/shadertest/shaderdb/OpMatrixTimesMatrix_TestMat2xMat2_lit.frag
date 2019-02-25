#version 450 core

layout(location = 0) in vec2 l0 ;
layout(location = 1) in vec2 l1 ;
layout(location = 2) in vec2 l2 ;
layout(location = 0) out vec4 color;


void main()
{
    mat2 x = outerProduct(l0,l1);
    mat2 y = outerProduct(l0,l2);
    mat2 z = x * y;
    color.xy = z[0];
    color.wz = z[1];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: [2 x <2 x float>] @_Z17MatrixTimesMatrixDv2_Dv2_fDv2_Dv2_f

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: fmul <2 x float> %{{.*}}, %{{[0-9]*}}
; SHADERTEST: fadd <2 x float> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
