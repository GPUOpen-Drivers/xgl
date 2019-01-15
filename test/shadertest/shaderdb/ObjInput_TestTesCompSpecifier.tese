#version 450 core

layout(triangles) in;

layout(location = 0) patch in vec2 f2;
layout(location = 0, component = 2) patch in float f1;

layout(location = 0) out vec3 outColor;

void main(void)
{
    outColor = vec3(f2, f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
