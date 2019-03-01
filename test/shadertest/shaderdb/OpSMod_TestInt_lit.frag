#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 color;
void main()
{
    int ua = int(a0.x);
    int ub = int(b0.x);
    int uc = ua % ub;
    int uc0 = 12 % -5;
    int uc1 = 10 % 5;
    int uc2 = 10 % 3;

    color = vec4(uc, uc0, uc1, uc2);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: i32 @_Z4smodii

; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: srem i32
; SHADERTEST: insertelement <4 x float> <float undef, float 2.000000e+00, float 0.000000e+00, float 1.000000e+00>, float %{{.*}}, i32 0

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
