#version 450 core

struct S
{
    vec4 f4;
};

layout(std430, binding = 0) buffer Block
{
    vec3 f3;
    S    s;
} block;

void main()
{
    S s;
    s.f4 = vec4(1.0);

    block.s = s;

    gl_Position = vec4(1.0);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: getelementptr { <3 x float>, { <4 x float> } }, { <3 x float>, { <4 x float> } } addrspace({{.*}})* @{{.*}}, i32 0, i32 1
; SHADERTEST: getelementptr { <4 x float> }, { <4 x float> } addrspace({{.*}})* %{{[0-9]*}}, i32 0, i32 0

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16, <16 x i8> <i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63, i8 0, i8 0, i8 -128, i8 63>, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
