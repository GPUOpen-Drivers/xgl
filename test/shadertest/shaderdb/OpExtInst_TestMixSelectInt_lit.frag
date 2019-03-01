#version 450

layout(binding = 0) uniform Uniforms
{
    int i1_1, i1_2;
    bool b1;

    ivec3 i3_1, i3_2;
    bvec3 b3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    int i1_0 = mix(i1_1, i1_2, b1);

    ivec3 i3_0 = mix(i3_1, i3_2, b3);

    fragColor = (i3_0.y == i1_0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = select i1 %{{.*}}, i32 %{{.*}}, i32 %{{.*}}
; SHADERTEST: %{{[0-9]*}} = select <3 x i1> %{{.*}}, <3 x i32> %{{.*}}, <3 x i32> %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
