#version 450 core

layout(push_constant) uniform PCB
{
    vec4 m0;
    vec4 m1[16];
} g_pc;

void main()
{
    gl_Position = g_pc.m1[8];
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call [512 x i8] addrspace({{.*}})* {{.*}}@llpc.call.desc.load.spill.{{[0-9a-z.]*}}{{.*}}
; SHADERTEST: call <16 x i8> @llpc.pushconst.load.v16i8([512 x i8] addrspace({{.*}})* %0, i32 144{{.*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
