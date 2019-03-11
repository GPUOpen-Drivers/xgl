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
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f32(i32 0, i32 1, i32 0, <3 x float> <float 3.000000e+00, float 3.000000e+00, float 3.000000e+00>)
; SHADERTEST: call void @llpc.output.export.generic{{.*}}f32(i32 0, i32 0, i32 0, float 1.500000e+00)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
