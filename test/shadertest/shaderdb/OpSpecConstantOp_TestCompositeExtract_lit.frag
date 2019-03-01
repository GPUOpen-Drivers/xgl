#version 450 core

layout(constant_id = 2) const int  sci = -2;

const ivec3 iv3 = ivec3(sci, -7, -8);
const ivec2 iv2 = iv3.yx;
const int   i   = iv2[1];

layout(location = 0) out vec4 f3;

void main()
{
    f3 = vec4(i);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: insertelement <4 x float> %{{[0-9]*}}, float %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
