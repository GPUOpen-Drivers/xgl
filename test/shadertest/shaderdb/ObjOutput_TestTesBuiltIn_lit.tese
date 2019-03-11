#version 450 core

#extension GL_ARB_shader_viewport_layer_array: enable

layout(triangles) in;

layout(location = 1) in vec4 inColor[];

void main()
{
    gl_Position = inColor[1];
    gl_PointSize = inColor[2].x;
    gl_ClipDistance[2] = inColor[0].y;
    gl_CullDistance[3] = inColor[0].z;

    gl_Layer = 2;
    gl_ViewportIndex = 1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.builtin.Position{{.*}}v4f32
; SHADERTEST: call void @llpc.output.export.builtin.PointSize{{.*}}f32
; SHADERTEST: call void @llpc.output.export.builtin.ClipDistance{{.*}}a3f32
; SHADERTEST: call void @llpc.output.export.builtin.CullDistance{{.*}}a4f32
; SHADERTEST: call void @llpc.output.export.builtin.Layer{{.*}}
; SHADERTEST: call void @llpc.output.export.builtin.ViewportIndex{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
