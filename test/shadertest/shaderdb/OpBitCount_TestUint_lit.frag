#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1;
    uvec3 u3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_0 = bitCount(u1_1);

    ivec3 i3_0 = bitCount(u3_1);

    fragColor = (i1_0 != i3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST-COUNT-2: call i32 @llvm.ctpop.i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
