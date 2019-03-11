#version 450 core

layout(std430, binding = 0) buffer Block
{
    vec4   base;
    vec4   o[];
} block;

void main()
{
    block.o[gl_VertexIndex] = block.base + vec4(gl_VertexIndex);

    gl_Position = vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: getelementptr { <4 x float>, [4294967295 x <4 x float>] }, { <4 x float>, [4294967295 x <4 x float>] } addrspace({{.*}})* @{{.*}}, i32 0, i32 1, i32 %{{[0-9]*}}

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, <16 x i8> %{{[0-9]*}}, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
