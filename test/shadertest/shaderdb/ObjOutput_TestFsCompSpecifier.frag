#version 450 core

layout(location = 0) in vec3 f3;

layout(location = 0, component = 1) out vec2  f2;
layout(location = 0, component = 3) out float f1;

void main (void)
{
    f2 = vec2(f3.y, 2.0);
    f1 = f3.x;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
