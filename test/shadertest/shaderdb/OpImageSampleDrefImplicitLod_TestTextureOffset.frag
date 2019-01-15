#version 450

layout(set = 0, binding = 0) uniform sampler1DShadow    samp1DShadow;
layout(set = 1, binding = 0) uniform sampler2DShadow    samp2DShadow[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1 = textureOffset(samp1DShadow, vec3(0.8), 2, 0.3);
    f1 += textureOffset(samp2DShadow[index], vec3(0.5), ivec2(5));

    fragColor = vec4(f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
