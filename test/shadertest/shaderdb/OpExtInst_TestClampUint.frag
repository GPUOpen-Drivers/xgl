#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1, u1_2;
    uvec3 u3_1, u3_2, u3_3;
    uvec4 u4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3_0 = clamp(u3_1, u3_2, u3_3);

    uvec4 u4_0 = clamp(u4_1, u1_1, u1_2);

    fragColor = (u3_0.y == u4_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
