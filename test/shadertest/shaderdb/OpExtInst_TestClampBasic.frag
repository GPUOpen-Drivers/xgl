#version 450 core

layout(location = 0) in vec4 a;
layout(location = 1) in vec4 b;
layout(location = 2) in vec4 c;


layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fclamp = clamp(a, b, c);
    ivec4 iclamp = clamp(ivec4(a), ivec4(b), ivec4(c));
    uvec4 uclamp = clamp(uvec4(a), uvec4(b), uvec4(c));
    frag_color = fclamp + vec4(iclamp) + vec4(uclamp);
}


// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
