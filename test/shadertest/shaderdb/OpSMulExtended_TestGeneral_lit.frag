#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_1, i1_2;
    ivec3 i3_1, i3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_3, i1_4;
    imulExtended(i1_1, i1_2, i1_3, i1_4);

    ivec3 i3_3, i3_4;
    imulExtended(i3_1, i3_2, i3_3, i3_4);

    fragColor = ((i1_3 == i1_4) || (i3_3 == i3_4)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: { i32, i32 } @_Z12SMulExtendedii
; SHADERTEST: { <3 x i32>, <3 x i32> } @_Z12SMulExtendedDv3_iDv3_i

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: mul nsw i64 %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
