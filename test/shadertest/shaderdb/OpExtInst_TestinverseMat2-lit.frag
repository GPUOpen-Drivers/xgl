#version 450
layout(location = 0) in mat2 m0;
layout(location = 0) out vec4 o_color;

void main (void)
{
	mat2 newm = inverse(m0);
	o_color = vec4(newm[0][0], newm[0][1], newm[1][0], newm[1][1]);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} [2 x <2 x float>] @_Z13matrixInverseDv2_Dv2_f([2 x <2 x float>] %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
