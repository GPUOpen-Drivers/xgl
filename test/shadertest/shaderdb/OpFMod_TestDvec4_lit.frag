#version 450

layout(binding = 0) uniform Uniforms
{
    dvec4  d4_1;
    double d1;
    double d2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec4 d4_0 = dvec4(d2);
    d4_0 = mod(d4_0, d4_1);

    d4_0 += mod(d4_0, d1);

    fragColor = (d4_0.y > 0.0) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-COUNT-2: call spir_func <4 x double> @_Z4fmodDv4_dDv4_d
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
