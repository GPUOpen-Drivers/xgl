#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    uvec4 bd = uvec4(colorIn1);
    uint outi = 0;
    uint out1 = usubBorrow(bd.x,bd.y,outi);
    color = vec4(out1 + outi);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: { i32, i32 } @_Z10ISubBorrowii

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call { i32, i1 } @llvm.usub.with.overflow.i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
