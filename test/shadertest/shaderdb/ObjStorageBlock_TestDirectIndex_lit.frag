#version 450

layout(std430, column_major, set = 0, binding = 1) buffer BufferObject
{
    uint ui;
    vec4 v4;
    vec4 v4_array[2];
} ssbo[2];

layout(location = 0) out vec4 output0;

void main()
{
    output0 = ssbo[0].v4_array[1];

    ssbo[1].ui = 0;
    ssbo[1].v4 = vec4(3);
    ssbo[1].v4_array[0] = vec4(4);
    ssbo[1].v4_array[1] = vec4(5);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 0
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 48
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 1
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 0
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 1
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 16,
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 1
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 32
; SHADERTEST: call <4 x i32> {{.*}}@llpc.call.desc.load.buffer.{{[0-9a-z.]*}}(i32 0, i32 1, i32 1
; SHADERTEST: call void @llpc.buffer.store.v16i8(<4 x i32> %{{[0-9]*}}, i32 48

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
