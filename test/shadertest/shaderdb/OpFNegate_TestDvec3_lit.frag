#version 450

layout(binding = 0) uniform Uniforms
{
    dvec3 d3_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    dvec3 d3_1 = -d3_0;

    fragColor = vec4(d3_1, 0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fsub <3 x double> <double -0.000000e+00, double -0.000000e+00, double -0.000000e+00>
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST-COUNT-3: fsub float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
