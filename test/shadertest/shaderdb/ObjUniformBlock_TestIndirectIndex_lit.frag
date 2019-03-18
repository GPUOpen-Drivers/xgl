#version 450

layout(set = 0, binding = 1) uniform BB
{
   vec4 m1;
   vec4 m2[10];
} b[4];

layout(binding = 0) uniform Uniforms
{
    int i;
    int j;
};

layout(location = 0) out vec4 o1;

void main()
{
    o1 = b[0].m1 + b[1].m2[i] + b[j].m2[3]+ b[i].m2[j];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 0, i1 false)
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 0, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 0, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 0, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 1, i1 false)
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 0, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 4, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 %{{[0-9]*}}, i1 false)
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 64, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 0, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 0, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 0, i32 0, i1 false)
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8(<4 x i32> %{{[0-9]*}}, i32 4, i1 true, i32 0, i1 false)
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 %{{[0-9]*}}, i1 false)
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i1 true, i32 0, i1 false)

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 0, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 0)
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32(<4 x i32> %{{[0-9]*}}, i32 4, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 64, i32 0)
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32(<4 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 0)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
