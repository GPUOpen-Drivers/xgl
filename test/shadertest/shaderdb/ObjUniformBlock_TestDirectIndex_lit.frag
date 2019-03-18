#version 450

layout(set = 1, binding = 0) uniform BB
{
   vec4 m1;
   vec4 m2[10];
} b[4];


layout(location = 0) out vec4 o1;

void main()
{
    o1 = b[0].m1 + b[3].m2[5];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: getelementptr [4 x { <4 x float>, [10 x <4 x float>] }], [4 x { <4 x float>, [10 x <4 x float>] }] addrspace({{.*}})* @{{.*}}, i32 0, i32 0, i32 0
; SHADERTEST: getelementptr [4 x { <4 x float>, [10 x <4 x float>] }], [4 x { <4 x float>, [10 x <4 x float>] }] addrspace({{.*}})* @{{.*}}, i32 0, i32 3, i32 1, i32 5

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 1, i32 0, i32 0
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[[0-9]*}}, i32 0
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 1, i32 0, i32 3
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[[0-9]*}}, i32 96

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 96, i32 0)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
