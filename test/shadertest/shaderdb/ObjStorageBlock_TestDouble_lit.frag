#version 450

layout(std430, column_major, set = 0, binding = 0) buffer BufferObject
{
    uint ui;
    double d;
    dvec4 dv4;
    dmat2x4 dm2x4;
};

layout(location = 0) out vec4 output0;

void main()
{
    dvec4 dv4temp = dv4;
    dv4temp.x = d;
    d = dv4temp.y;
    output0 = vec4(dv4temp);
    dv4temp = dm2x4[0];
    dm2x4[1] = dv4temp;
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <32 x i8> @llpc.buffer.load.v32i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call void @llpc.buffer.store.v8i8(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call <32 x i8> @llpc.buffer.load.v32i8(<4 x i32> %{{[0-9]*}}, i32 64
; SHADERTEST: call void @llpc.buffer.store.v32i8(<4 x i32> %{{[0-9]*}}, i32 96

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: call <4 x float> @llvm.amdgcn.raw.buffer.load.v4f32(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call <4 x float> @llvm.amdgcn.raw.buffer.load.v4f32(<4 x i32> %{{[0-9]*}}, i32 48
; SHADERTEST: call <2 x float> @llvm.amdgcn.raw.buffer.load.v2f32(<4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v2f32(<2 x float> %{{[0-9]*}}.i1.upto1, <4 x i32> %{{[0-9]*}}, i32 8
; SHADERTEST: call <4 x float> @llvm.amdgcn.raw.buffer.load.v4f32(<4 x i32> %{{[0-9]*}}, i32 64
; SHADERTEST: call <4 x float> @llvm.amdgcn.raw.buffer.load.v4f32(<4 x i32> %{{[0-9]*}}, i32 80
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 96
; SHADERTEST: call void @llvm.amdgcn.raw.buffer.store.v4f32(<4 x float> %{{[0-9]*}}, <4 x i32> %{{[0-9]*}}, i32 112


; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
