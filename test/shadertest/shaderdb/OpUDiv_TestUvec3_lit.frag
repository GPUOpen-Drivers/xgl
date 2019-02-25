#version 450

layout(binding = 0) uniform Uniforms
{
    uvec3 u3_0;
    uvec3 u3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3_2 = u3_0 / u3_1;

    fragColor = (u3_2.y != 2) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: udiv <3 x i32>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
