#version 450

layout(binding = 0) uniform Uniforms
{
    ivec3 i3;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec3 u3 = i3;

    fragColor = (u3.x != u3.y) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
; SHADERTEST: bitcast <12 x i8> %{{[0-9]*}} to <3 x i32>

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
