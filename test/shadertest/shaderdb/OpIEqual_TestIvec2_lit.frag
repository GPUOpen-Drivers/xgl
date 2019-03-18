#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2_0, i2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (i2_0 == i2_1) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}}  SPIR-V lowering results
; SHADERTEST: icmp eq <2 x i32>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
