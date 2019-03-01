#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    uvec4 u4 = uvec4(5);
    uint u1 = 90;
    u4 *= u1;

    fragColor = (u4 == uvec4(6)) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{.*}} = insertelement <4 x i32> undef, i32 %{{.*}}, i32 0
; SHADERTEST: %{{.*}} = insertelement <4 x i32> %{{.*}}, i32 %{{.*}}, i32 1
; SHADERTEST: %{{.*}} = insertelement <4 x i32> %{{.*}}, i32 %{{.*}}, i32 2
; SHADERTEST: %{{.*}} = insertelement <4 x i32> %{{.*}}, i32 %{{.*}}, i32 3
; SHADERTEST: %{{.*}} = mul <4 x i32> %{{.*}}, %{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
