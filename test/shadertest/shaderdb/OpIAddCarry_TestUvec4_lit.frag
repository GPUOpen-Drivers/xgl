#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    uvec4 bd = uvec4(colorIn1);
    uint outi = 0;
    uvec4 outv = uvec4(0);
    uint out1 = uaddCarry(bd.x,bd.y,outi);
    uvec4 out0 = uaddCarry(bd,bd,outv);
    color = vec4(out0 + outv);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call spir_func { i32, i32 } @_Z9IAddCarryii
; SHADERTEST: call spir_func { <4 x i32>, <4 x i32> } @_Z9IAddCarryDv4_iDv4_i
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST-COUNT-4: call { i32, i1 } @llvm.uadd.with.overflow.i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
