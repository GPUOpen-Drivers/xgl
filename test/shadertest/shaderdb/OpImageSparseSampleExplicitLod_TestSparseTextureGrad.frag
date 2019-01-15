#version 450
#extension GL_ARB_sparse_texture2: enable

layout(set = 0, binding = 0) uniform sampler2D      samp2D;
layout(set = 1, binding = 0) uniform sampler3D      samp3D[4];
layout(set = 2, binding = 0) uniform sampler2DRect  samp2DRect;

layout(set = 3, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseTextureGradARB(samp2D, vec2(0.1), vec2(1.0), vec2(1.1), texel);
    fragColor += texel;

    sparseTextureGradARB(samp3D[index], vec3(0.2), vec3(1.0), vec3(1.1), texel);
    fragColor += texel;

    sparseTextureGradOffsetARB(samp2DRect, vec2(0.3), vec2(1.0), vec2(1.1), ivec2(2), texel);
    fragColor += texel;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
