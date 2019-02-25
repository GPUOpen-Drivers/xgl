#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec4 i4 = ivec4(f4);

    fragColor = (i4.x != 2) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fptosi <4 x float> {{.*}} to <4 x i32>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
