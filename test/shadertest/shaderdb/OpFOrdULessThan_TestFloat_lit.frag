#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    bvec4 x = lessThan (uvec4(colorIn1), uvec4(colorIn2));
    color = vec4(x);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST: icmp ult <4 x i32>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST-COUNT-4: icmp ult i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
