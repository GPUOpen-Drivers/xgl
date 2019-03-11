#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1, u1_2;
    uvec3 u3_1, u3_2;

    int i1_1, i1_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_0 = bitfieldInsert(u1_1, u1_2, i1_1, i1_2);

    uvec3 u3_0 = bitfieldInsert(u3_1, u3_2, i1_1, i1_2);

    fragColor = (u1_0 != u3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldInsert{{.*}}(i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})
; SHADERTEST: call {{.*}} <3 x i32> {{.*}}BitFieldInsert{{.*}}(<3 x i32> %{{[0-9]*}}, <3 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
