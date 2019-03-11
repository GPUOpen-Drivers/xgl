#version 450

layout(set = 1, binding = 0) coherent buffer Buffer
{
    readonly vec4 f4;
    restrict vec2 f2;
    volatile writeonly float f1;
} buf;

void main()
{
    vec4 texel = vec4(0.0);

    texel += buf.f4;
    texel.xy += buf.f2;
    buf.f1 = texel.w;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.buffer.load.v16i8(<4 x i32> %{{[0-9]*}}, i32 0, i1 true, i32 1, i1 false)
; SHADERTEST: call <8 x i8> @llpc.buffer.load.v8i8(<4 x i32> %{{[0-9]*}}, i32 16, i1 false, i32 1, i1 false)
; SHADERTEST: call void @llpc.buffer.store.v4i8(<4 x i32> %{{[0-9]*}}, i32 24, <4 x i8> %{{[0-9]*}}, i32 3, i1 false)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
