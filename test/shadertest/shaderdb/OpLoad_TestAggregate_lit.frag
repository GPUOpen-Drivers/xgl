#version 450

struct AggregateType
{
    vec4    v1;
    float   v2;
    vec2    v3;
    mat2    v4;
    vec4    v5[2];
};

layout(set = 0, binding = 0) uniform UBO
{
    AggregateType m1;
};

layout(set = 1, binding = 0) uniform Uniforms
{
    AggregateType c1;
    int i;
};

AggregateType temp1;
AggregateType temp2;

layout(location = 0) out vec4 o1;
layout(location = 1) out vec4 o2;
layout(location = 2) out vec4 o3;

void main()
{
    temp1 = m1;
    temp2 = c1;
    o1 = temp1.v5[i];
    o2 = m1.v5[i];
    o3 = c1.v1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: load

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8
; SHADERTEST: call <4 x i8> @llpc.buffer.load.v4i8
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x i32> @llvm.amdgcn.s.buffer.load.v4i32
; SHADERTEST: call i32 @llvm.amdgcn.s.buffer.load.i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
