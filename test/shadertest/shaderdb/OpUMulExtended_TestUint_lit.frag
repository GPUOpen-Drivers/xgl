#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    uvec4 bd = uvec4(colorIn1);
    uint outi = 0;
    uint out1 = 0;
    umulExtended(bd.x,bd.y,outi, out1);
    color = vec4(out1 + outi);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { i32, i32 } @_Z12UMulExtendedii(i32 %{{[0-9]*}}, i32 %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = mul nuw i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
