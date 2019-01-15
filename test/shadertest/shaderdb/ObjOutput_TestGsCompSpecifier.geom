#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0, component = 1) out vec3 f3;
layout(location = 0, component = 0) out float f1;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        f3 = vec3(3.0);
        f1 = 1.5;
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
