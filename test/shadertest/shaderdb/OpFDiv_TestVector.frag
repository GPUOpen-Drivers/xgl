#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4_1;
    dvec2 d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4_0 = vec4(0.0);
    f4_0 /= f4_1;

    dvec2 d2_0 = dvec2(0.0);
    d2_0 /= d2_1;

    fragColor = ((f4_0.y > 0.4) || (d2_0.x != d2_0.y)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
