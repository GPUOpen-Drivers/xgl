#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    ivec2 i2 = ivec2(0);
    i2 = i2 << 3;

    fragColor = (i2.x != 4) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: shl <2 x i32>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
