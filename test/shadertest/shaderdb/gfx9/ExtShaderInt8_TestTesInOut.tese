#version 450 core

#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable

layout(triangles) in;

layout(location = 0) in uint8_t u8In[];
layout(location = 1) in i8vec3 i8v3In[];

layout(location = 0) out uint8_t u8Out;
layout(location = 1) out i8vec3 i8v3Out;

void main(void)
{
    u8Out = u8In[0] + u8In[1] + u8In[2];
    i8v3Out = i8v3In[0] + i8v3In[1] + i8v3In[2];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST