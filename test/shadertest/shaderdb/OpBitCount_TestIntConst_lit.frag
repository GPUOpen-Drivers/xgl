#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    color = vec4(bitCount(bd.x) + bitCount(384));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call i32 @llvm.ctpop.i32
; SHADERTEST: add nuw nsw i32 %{{[0-9*]}}, 2

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
