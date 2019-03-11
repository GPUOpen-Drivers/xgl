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
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call float @llpc.input.import.generic.f32.i32.i32.i32(i32 0, i32 0, i32 0)
; SHADERTEST: call float @llpc.input.import.generic.f32.i32.i32.i32(i32 0, i32 0, i32 1)
; SHADERTEST: call float @llpc.input.import.generic.f32.i32.i32.i32(i32 0, i32 0, i32 2)
; SHADERTEST: call <3 x float> @llpc.input.import.generic.v3f32.i32.i32.i32(i32 0, i32 1, i32 0)
; SHADERTEST: call <3 x float> @llpc.input.import.generic.v3f32.i32.i32.i32(i32 0, i32 1, i32 1)
; SHADERTEST: call <3 x float> @llpc.input.import.generic.v3f32.i32.i32.i32(i32 0, i32 1, i32 2)
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
