#version 450 core
layout(location = 0) in vec4 incolor;
layout(location = 0) out vec4 color;

void main()
{
    vec4 v = vec4(incolor.x);
    vec4 y = vec4(incolor.y);
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
