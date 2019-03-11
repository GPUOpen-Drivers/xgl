#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1;
    uvec3 u3_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_0 = bitfieldReverse(u1_1);

    uvec3 u3_0 = bitfieldReverse(u3_1);

    fragColor = (u1_0 != u3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: i32 {{.*}}BitReversei(i32 %{{[0-9]*}})
; SHADERTEST: <3 x i32> {{.*}}BitReverseDv3_i(<3 x i32> %{{[0-9]*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
