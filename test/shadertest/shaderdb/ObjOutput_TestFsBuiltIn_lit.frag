#version 450 core

#extension GL_ARB_shader_stencil_export: enable

layout(location = 0) in vec4 f4;

void main()
{
    gl_FragDepth = f4.x;
    gl_SampleMask[0] = 27;
    gl_FragStencilRefARB = 32;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.builtin.FragDepth{{.*}}f32
; SHADERTEST: call void @llpc.output.export.builtin.SampleMask.i32.a1i32
; SHADERTEST: call void @llpc.output.export.builtin.FragStencilRefEXT{{.*}}
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call void @llvm.amdgcn.exp.f32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
