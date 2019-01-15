#version 450

layout(std430, column_major, set = 0, binding = 0) buffer BufferObject
{
    uint ui;
    vec4 v4;
    vec4 v4_array[2];
}ssbo[2];

layout(set = 1, binding = 0) uniform Uniforms
{
    int i;
    int j;
};

layout(location = 0) out vec4 output0;

void main()
{
    output0 = ssbo[i].v4_array[j];

    ssbo[i].ui = 0;
    ssbo[i].v4 = vec4(3);
    ssbo[i].v4_array[j] = vec4(4);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
