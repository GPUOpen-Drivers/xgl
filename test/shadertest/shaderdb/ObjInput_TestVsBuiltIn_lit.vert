#version 450 core

layout(location = 0) out int i1;

void main()
{
    i1 = gl_VertexIndex + gl_InstanceIndex;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.input.import.builtin.InstanceIndex{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.VertexIndex{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
