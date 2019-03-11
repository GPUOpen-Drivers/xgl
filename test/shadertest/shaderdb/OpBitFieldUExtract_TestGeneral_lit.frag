#version 450

layout(binding = 0) uniform Uniforms
{
    uint u1_1;
    uvec3 u3_1;

    int i1_1, i1_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uint u1_0 = bitfieldExtract(u1_1, i1_1, i1_2);

    uvec3 u3_0 = bitfieldExtract(u3_1, i1_1, i1_2);

    fragColor = (u1_0 != u3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldUExtract{{.*}}(i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})
; SHADERTEST: call {{.*}} <3 x i32> {{.*}}BitFieldUExtract{{.*}}(<3 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-COUNT-2: call i32 @llvm.amdgcn.ubfe.i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
