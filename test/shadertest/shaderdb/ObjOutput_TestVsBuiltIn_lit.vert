#version 450 core

#extension GL_ARB_shader_viewport_layer_array: enable

void main()
{
    gl_Position = vec4(1.0);
    gl_PointSize = 5.0;
    gl_ClipDistance[3] = 1.5;
    gl_CullDistance[1] = 2.0;

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
; SHADERTEST: call void @llpc.output.export.builtin.ClipDistance{{.*}}a4f32
; SHADERTEST: call void @llpc.output.export.builtin.CullDistance{{.*}}a2f32
; SHADERTEST: call void @llpc.output.export.builtin.Layer{{.*}}
; SHADERTEST: call void @llpc.output.export.builtin.ViewportIndex{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
