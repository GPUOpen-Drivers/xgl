#version 450

layout(location = 0) out vec4 f4;
#extension GL_EXT_multiview : enable

void main()
{
    vec4 f = vec4(0.0);

    f.x   += float(gl_SampleMaskIn[0]);
    f     += gl_FragCoord;
    f.x   += float(gl_FrontFacing);
    f.x   += gl_ClipDistance[3];
    f.x   += gl_CullDistance[2];
    f.xy  += gl_PointCoord;
    f.x   += float(gl_PrimitiveID);
    f.x   += float(gl_SampleID);
    f.x   += float(gl_SampleMaskIn[0]);
    f.x   += float(gl_Layer);
    f.x   += float(gl_ViewportIndex);
    f.x   += float(gl_HelperInvocation);
    f.x   += float(gl_ViewIndex);

    f4 = f;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call i32 @llpc.input.import.builtin.ViewIndex{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.HelperInvocation{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.ViewportIndex{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.Layer{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.SampleId{{.*}}
; SHADERTEST: call i32 @llpc.input.import.builtin.PrimitiveId{{.*}}
; SHADERTEST: call <2 x float> @llpc.input.import.builtin.PointCoord.v2f32.i32
; SHADERTEST: call [3 x float] @llpc.input.import.builtin.CullDistance.a3f32.i32
; SHADERTEST: call [4 x float] @llpc.input.import.builtin.ClipDistance.a4f32.i32
; SHADERTEST: call i32 @llpc.input.import.builtin.FrontFacing{{.*}}
; SHADERTEST: call <4 x float> @llpc.input.import.builtin.FragCoord.v4f32.i32
; SHADERTEST: call [1 x i32] @llpc.input.import.builtin.SampleMask.a1i32.i32
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
