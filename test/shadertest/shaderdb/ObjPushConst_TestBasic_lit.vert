#version 450 core

layout(push_constant) uniform PCB
{
    vec4 m1;
} g_pc;

void main()
{
    gl_Position = g_pc.m1;
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.pushconst.load.v16i8

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
