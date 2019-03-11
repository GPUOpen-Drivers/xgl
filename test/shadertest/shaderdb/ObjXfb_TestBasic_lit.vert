#version 450


layout(xfb_buffer = 0, xfb_offset = 16, xfb_stride = 32, location = 0) out vec4 output1;

void main()
{
    gl_Position = vec4(1);
    output1 = vec4(2);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call void @llpc.output.export.xfb.i32.i32.i32.v4f32(i32 0, i32 0, i32 0, <4 x float> <float 1.000000e+00, float 1.000000e+00, float 1.000000e+00, float 1.000000e+00>)
; SHADERTEST: call void @llpc.output.export.xfb.i32.i32.i32.f32(i32 0, i32 0, i32 0
; SHADERTEST: call void @llpc.output.export.xfb.i32.i32.i32.v4f32(i32 0, i32 16, i32 0, <4 x float> <float 2.000000e+00, float 2.000000e+00, float 2.000000e+00, float 2.000000e+00>)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
