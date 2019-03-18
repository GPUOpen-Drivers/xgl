#version 450 core

layout(location = 0) in float a;
layout(location = 1) in vec4 b0;


layout(location = 0) out vec4 frag_color;
void main()
{
    frag_color = vec4(mod(b0.x,a));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call spir_func float @_Z4fmodff
; SHADERTEST-LABEL: {{^// LLPC}}  pipeline patching results
; SHADERTEST: fdiv float
; SHADERTEST: fmul float
; SHADERTEST: call float @llvm.floor.f32
; SHADERTEST: fsub float
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
