#version 450 core

struct Str
{
    float m0;
    float m1;
};

layout (push_constant) uniform UBO
{
    layout (offset = 16)    vec4 m0;
    layout (offset = 4)     Str  m1;
} g_pc;

void main()
{
    gl_Position = vec4(g_pc.m1.m1);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <4 x i8> @llpc.pushconst.load.v4i8({{.*}}i32 8,{{.*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
