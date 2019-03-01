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

    b2 = greaterThan(i2_0, i2_1);

    b2 = greaterThan(u2_0, u2_1);

    fragColor = (b2.x) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: icmp sgt
; SHADERTEST: icmp ugt

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
