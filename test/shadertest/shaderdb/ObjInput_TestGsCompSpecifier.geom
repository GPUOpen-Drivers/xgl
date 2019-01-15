#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0, component = 1) in vec3 f3[];
layout(location = 0, component = 0) in float f1[];

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        gl_Position = vec4(f3[i], f1[i]);
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
