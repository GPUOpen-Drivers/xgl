#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 f4 = vec4(0.0);
    f4[2] = 0.5;

    dvec3 d3 = dvec3(0.0);
    d3[2] = d3[0];

    fragColor = ((f4.x > 0.0) && (d3.x == d3.y)) ? vec4(1.0) : vec4(0.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
