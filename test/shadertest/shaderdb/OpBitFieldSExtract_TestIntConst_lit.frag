#version 450 core

layout(location = 0) in vec4 colorIn1;
layout(location = 0) out vec4 color;
void main()
{
    ivec4 bd = ivec4(colorIn1);
    int b0 = bitfieldExtract(bd.x, bd.x, bd.y);
    int b1 = bitfieldExtract(3423, 0, 32);
    color = vec4(b0 + b1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldSExtract{{.*}}(i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldSExtract{{.*}}(i32 3423, i32 0, i32 32)

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call i32 @llvm.amdgcn.sbfe.i32
; SHADERTEST: add i32 %{{[0-9]*}}, 3423

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
