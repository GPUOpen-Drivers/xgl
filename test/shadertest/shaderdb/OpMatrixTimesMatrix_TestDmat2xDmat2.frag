#version 450

layout(binding = 0) uniform Uniforms
{
    dmat2 dm2_0;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 color = vec4(0.5);

    dmat2 dm2_1 = dmat2(0.0);

    dm2_1 = dm2_0 * dm2_1;

    fragColor = (dm2_1[0] == dm2_0[1]) ? vec4(1.0) : color;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
