#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_3;
    ivec3 i3_1;

    int i1_1, i1_2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_0 = bitfieldExtract(i1_3, i1_1, i1_2);

    ivec3 i3_0 = bitfieldExtract(i3_1, i1_1, i1_2);

    fragColor = (i1_0 != i3_0.x) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} i32 {{.*}}BitFieldSExtract{{.*}}(i32 %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})
; SHADERTEST: call {{.*}} <3 x i32> {{.*}}BitFieldSExtract{{.*}}(<3 x i32> %{{[0-9]*}}, i32 %{{[0-9]*}}, i32 %{{[0-9]*}})

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST-CONUT-2: call i32 @llvm.amdgcn.sbfe.i32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
