#version 450 core

layout(vertices = 3) out;

layout(location = 1) out vec4 outColor[];

void main (void)
{
    outColor[gl_InvocationID] = gl_in[gl_InvocationID].gl_Position;
    outColor[gl_InvocationID].x += gl_in[gl_InvocationID].gl_PointSize;
    outColor[gl_InvocationID].y += gl_in[gl_InvocationID].gl_ClipDistance[2];
    outColor[gl_InvocationID].z += gl_in[gl_InvocationID].gl_CullDistance[3];

    int value = gl_PatchVerticesIn + gl_PrimitiveID;
    outColor[gl_InvocationID].w += float(value);
    outColor[gl_InvocationID].w += gl_in[gl_InvocationID].gl_Position.z;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST-COUNT-2: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST-COUNT-1: call <4 x float> @llpc.input.import.builtin.Position.v4f32{{.*}}
; SHADERTEST-COUNT-2: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST-COUNT-1: call float @llpc.input.import.builtin.PointSize.f32{{.*}}
; SHADERTEST-COUNT-2: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST-COUNT-1: call float @llpc.input.import.builtin.ClipDistance.f32{{.*}}
; SHADERTEST-COUNT-2: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST-COUNT-1: call float @llpc.input.import.builtin.CullDistance.f32{{.*}}
; SHADERTEST-COUNT-1: call i32 @llpc.input.import.builtin.PatchVertices{{.*}}
; SHADERTEST-COUNT-1: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST-COUNT-3: call i32 @llpc.input.import.builtin.InvocationId{{.*}}
; SHADERTEST-COUNT-1: call float @llpc.input.import.builtin.Position.f32{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline linking results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
