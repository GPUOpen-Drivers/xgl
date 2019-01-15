#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2DShadow        samp2DShadow[4];

layout(set = 1, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    float texel = 0.0;

    sparseTextureLodARB(samp2DShadow[0], vec3(0.1), 1.0, texel);
    fragColor.x += texel;

    sparseTextureLodOffsetARB(samp2DShadow[index], vec3(0.2), 1.1, ivec2(1), texel);
    fragColor.y += texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
