#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 1) in vec4 colorIn2;
layout(location = 0) out vec4 color;
void main()
{
    bvec4 x = greaterThan (colorIn1, colorIn2);
    bvec4 y = greaterThan (uvec4(colorIn1), uvec4(colorIn2));
    bvec4 z = greaterThan (ivec4(colorIn1), ivec4(colorIn2));

    bvec4 w = equal(x,y);
    bvec4 q = notEqual(w,z);
    color = vec4(q);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST: icmp ugt <4 x i32>
; SHADERTEST: fcmp ule <4 x float>
; SHADERTEST: icmp sgt <4 x i32>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST-COUNT-4: icmp ugt
; SHADERTEST-COUNT-4: icmp sgt
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
