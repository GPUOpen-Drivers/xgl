#version 450 core

#extension GL_ARB_shader_group_vote: enable

layout(binding = 0) uniform Uniforms
{
    int i1;
    float f1;
};

layout(location = 0) out vec2 f2;

void main(void)
{
    bool b1 = false;

    switch (i1)
    {
    case 0:
        b1 = anyInvocationARB(f1 > 5.0);
        break;
    case 1:
        b1 = allInvocationsARB(f1 < 4.0);
        break;
    case 2:
        b1 = allInvocationsEqualARB(f1 != 0.0);
        break;
    }

    f2 = b1 ? vec2(1.0) : vec2(0.3);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i64 @llpc.subgroup.ballot
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
