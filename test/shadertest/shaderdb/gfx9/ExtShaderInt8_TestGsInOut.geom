#version 450 core

#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in uint8_t u8In[];
layout(location = 1) in i8vec3 i8v3In[];

layout(location = 0) out uint8_t u8Out;
layout(location = 1) out i8vec3 i8v3Out;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        u8Out = u8In[i];
        i8v3Out = i8v3In[i];
        EmitVertex();
    }

    EndPrimitive();
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST