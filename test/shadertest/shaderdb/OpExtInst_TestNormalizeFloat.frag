#version 450

layout(binding = 0) uniform Uniforms
{
    float f1_1;
    vec4 f4_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1_0 = normalize(f1_1);

    vec4 f4_0 = normalize(f4_1);

    fragColor = (f1_1 != f4_1.x) ? vec4(0.5) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
