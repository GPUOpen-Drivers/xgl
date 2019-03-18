#version 450

layout(binding = 0) uniform Uniforms
{
    vec4 f4_1;
    dvec2 d2_1;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4_0 = vec4(2.0);
    f4_0 /= f4_1;

    dvec2 d2_0 = dvec2(3.0);
    d2_0 /= d2_1;

    fragColor = ((f4_0.y > 0.4) || (d2_0.x != d2_0.y)) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-1: fdiv reassoc nnan arcp contract <4 x float>
; SHADERTEST-COUNT-1: fdiv reassoc nnan arcp contract <2 x double>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST: fdiv float
; SHADERTEST-COUNT-2: fdiv double
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
