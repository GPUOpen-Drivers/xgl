#version 450 core

layout(constant_id = 1) const int  sci = -2;

const ivec3 iv3 = ivec3(sci, -7, -8);
const ivec2 iv2 = iv3.zx;

layout(location = 0) out vec4 f3;

void main()
{
    f3 = vec4(iv2.x);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
