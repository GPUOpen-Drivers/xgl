#version 450
layout(location = 0) in mat4 m0;
layout(location = 0) out vec4 o_color;

void main (void)
{
	mat4 newm = inverse(m0);
	o_color = vec4(newm[0].xyzw + newm[1].xyzw + newm[2].xyzw + newm[3].xyzw);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
