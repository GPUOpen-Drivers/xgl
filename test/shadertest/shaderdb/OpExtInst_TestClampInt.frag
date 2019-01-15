#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_1, i1_2;
    ivec3 i3_1, i3_2, i3_3;
    ivec4 i4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec3 i3_0 = clamp(i3_1, i3_2, i3_3);

    ivec4 i4_0 = clamp(i4_1, i1_1, i1_2);

    fragColor = (i3_0.y == i4_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
