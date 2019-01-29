#version 450 core

#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable

layout(location = 0) flat in int8_t i8In;
layout(location = 1) flat in u8vec3 u8v3In;

layout(location = 0) out int8_t i8Out;
layout(location = 1) out u8vec3 u8v3Out;

void main (void)
{
    i8Out   = i8In;
    u8v3Out = u8v3In;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST