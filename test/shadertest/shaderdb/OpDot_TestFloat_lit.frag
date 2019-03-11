#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 10) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    vec2 a = a0.xy;
    vec2 b = b0.xy;
    color = vec4(dot(a0,b0));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]+}} = call {{[a-zA-Z_]+}} float @_Z3dotDv4_fDv4_f(<4 x float> %{{[0-9]+}}, <4 x float> %{{[0-9]+}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
