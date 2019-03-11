#version 450 core

#pragma use_storage_buffer

layout(std430, binding = 0) buffer Buffers
{
    vec4  f4;
    float f1[];
};

layout(location = 0) out vec4 f;

void main()
{
    f = f4 + vec4(f1.length());
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
: SHADERTEST: call i32 @llpc.buffer.arraylength(<4 x i32> %{{[0-9]*}}, i32 16, i32 4)

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
