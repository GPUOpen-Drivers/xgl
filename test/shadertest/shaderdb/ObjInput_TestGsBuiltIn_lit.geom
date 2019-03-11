#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 16) out;

layout(location = 2) out vec4 outColor;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        outColor    = gl_in[i].gl_Position;
        outColor.x += gl_in[i].gl_PointSize;
        outColor.y += gl_in[i].gl_ClipDistance[2];
        outColor.z += gl_in[i].gl_CullDistance[2];
        outColor.w += float(gl_PrimitiveIDIn + gl_InvocationID);

        EmitVertex();
    }

    EndPrimitive();
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.builtin.Position.v4f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.PointSize.f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.ClipDistance.a3f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.CullDistance.a3f32{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.builtin.Position.v4f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.PointSize.f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.ClipDistance.a3f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.CullDistance.a3f32{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.builtin.Position.v4f32{{.*}}
; SHADERTEST: call float @llpc.input.import.builtin.PointSize.f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.ClipDistance.a3f32{{.*}}
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.CullDistance.a3f32{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
