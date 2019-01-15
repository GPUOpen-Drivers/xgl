#version 450

layout(binding = 0) uniform Uniforms
{
    bvec4 b4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = (b4.x && b4.y) ? vec4(1.0) : vec4(0.5);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
