#version 450

layout(set = 0, binding = 0) uniform sampler1DShadow        samp1DShadow;
layout(set = 1, binding = 0) uniform samplerCubeArrayShadow sampCubeArrayShadow[4];

layout(set = 2, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    float f1 = texture(samp1DShadow, vec3(0.8), 1.0);
    f1 += texture(sampCubeArrayShadow[index], vec4(0.5), 0.6);

    fragColor = vec4(f1);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
