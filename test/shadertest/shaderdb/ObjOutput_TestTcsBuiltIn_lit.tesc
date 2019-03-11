#version 450 core

layout(vertices = 3) out;

layout(location = 1) in vec4 inColor[];

void main (void)
{
    gl_out[gl_InvocationID].gl_Position = inColor[gl_InvocationID];
    gl_out[gl_InvocationID].gl_PointSize = inColor[gl_InvocationID].x;
    gl_out[gl_InvocationID].gl_ClipDistance[2] = inColor[gl_InvocationID].y;
    gl_out[gl_InvocationID].gl_CullDistance[3] = inColor[gl_InvocationID].z;

    barrier();

    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[2] = 1.0;
    gl_TessLevelInner[0] = float(gl_PrimitiveID);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.builtin.Position{{.*}}v4f32
; SHADERTEST: call void @llpc.output.export.builtin.PointSize{{.*}}f32
; SHADERTEST: call void @llpc.output.export.builtin.ClipDistance{{.*}}f32
; SHADERTEST: call void @llpc.output.export.builtin.CullDistance{{.*}}f32
; SHADERTEST: call void @llpc.output.export.builtin.TessLevelOuter{{.*}}f32
; SHADERTEST: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST: call void @llpc.output.export.builtin.TessLevelInner{{.*}}f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
