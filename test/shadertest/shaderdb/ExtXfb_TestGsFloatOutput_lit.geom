#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in vec4 fIn[];
layout(location = 0, xfb_buffer = 1, xfb_offset = 24, stream = 0) out vec3 fOut1;
layout(location = 2, xfb_buffer = 0, xfb_offset = 16, stream = 1) out vec2 fOut2;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        fOut1 = fIn[i].xyz;
        EmitStreamVertex(0);

        fOut2 = fIn[i].xy;
        EmitStreamVertex(1);
    }

    EndPrimitive();
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v3f32
; SHADERTEST: call void @llpc.output.export.xfb{{.*}}v2f32
; SHADERTEST: call void @llpc.output.export.generic{{.*}}v2f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
