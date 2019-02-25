#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1, u1_2;
    uvec3 u3_1, u3_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_3, u1_4;
    umulExtended(u1_1, u1_2, u1_3, u1_4);

    uvec3 u3_3, u3_4;
    umulExtended(u3_1, u3_2, u3_3, u3_4);

    fragColor = ((u1_3 == u1_4) || (u3_3 == u3_4)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} { <3 x i32>, <3 x i32> } @_Z12UMulExtendedDv3_iDv3_i(<3 x i32> %{{[0-9]*}}, <3 x i32> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = mul nuw i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
