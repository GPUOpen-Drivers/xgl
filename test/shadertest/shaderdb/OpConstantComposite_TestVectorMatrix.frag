#version 450 core

layout(location = 0) out vec4 color;

void main()
{
    vec4 v = vec4(2.0);
    vec4 y = vec4(3.0);
    mat4 m = mat4(v,y,v,y);
    color = m[0];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
