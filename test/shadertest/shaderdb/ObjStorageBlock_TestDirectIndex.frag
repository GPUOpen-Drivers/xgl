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
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
