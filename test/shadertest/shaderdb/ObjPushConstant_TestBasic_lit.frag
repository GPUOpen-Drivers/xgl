#version 450

layout(binding = 1, std140, push_constant) uniform PushConstant
{
   vec4 m1;
   vec4 m2;
   vec4 m3;
   vec4 m4;
   vec4 m5;
   vec4 m6;
   vec4 m7;
   vec4 m8;
   vec4 m9;
   vec4 m10;
   vec4 m11;
   vec4 m12;
   vec4 m13;
   vec4 m14;
   vec4 m15;
   vec4 m16;
   vec4 m17;
   vec4 m18;
   vec4 m19;
   vec4 m20;
} pushConst;

layout(location = 0) out vec4 o1;

void main()
{
    o1 = pushConst.m5 + pushConst.m10;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: call <16 x i8> @llpc.pushconst.load.v16i8({{.*}}i32 64,{{.*}}
; SHADERTEST: call <16 x i8> @llpc.pushconst.load.v16i8({{.*}}i32 144,{{.*}}

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
