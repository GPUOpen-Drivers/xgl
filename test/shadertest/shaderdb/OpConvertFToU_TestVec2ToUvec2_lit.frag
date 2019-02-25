#version 450

layout(binding = 0) uniform Uniforms
{
    vec2 f2;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    uvec2 u2 = uvec2(f2);

    fragColor = (u2.x == u2.y) ? vec4(0.0) : vec4(1.0);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: fptoui <2 x float> {{.*}} to <2 x i32>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
