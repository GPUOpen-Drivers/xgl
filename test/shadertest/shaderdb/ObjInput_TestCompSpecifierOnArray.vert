#version 450 core

layout(location = 0, component = 1) in vec2 f2[4];

layout(location = 0) out vec3 color;

void main()
{
    vec3 f3 = vec3(1.0, f2[1]);
    color = f3;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
