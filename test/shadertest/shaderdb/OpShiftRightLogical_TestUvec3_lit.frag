#version 450

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3 = uvec3(0);
    u3 = u3 >> 5;

    fragColor = (u3.x != 4) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: lshr <3 x i32>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
