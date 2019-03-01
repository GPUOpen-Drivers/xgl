#version 450

layout(binding = 0) uniform Uniforms
{
    uvec2 u2_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec2 u2_1 = -u2_0;

    fragColor = (u2_1.x == 5) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: sub nsw <2 x i32> zeroinitializer, %{{[0-9]*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
