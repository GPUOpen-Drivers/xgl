#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2_0, i2_1;
    uvec2 u2_0, u2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    bvec2 b2;

    b2 = lessThan(i2_0, i2_1);

    b2 = lessThan(u2_0, u2_1);

    fragColor = (b2.x) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: icmp slt <2 x i32>
; SHADERTEST: icmp ult <2 x i32>

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: icmp ult <2 x i32>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
