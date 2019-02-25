#version 450

layout(binding = 0) uniform Uniforms
{
    ivec2 i2_0, i2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (i2_0 != i2_1) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = icmp ne <2 x i32> %{{[0-9]*}}, %{{[0-9]*}}

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: icmp ne i32 %{{.*}}, %{{.*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
