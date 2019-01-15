#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler2D          samp2D[4];
layout(set = 1, binding = 0) uniform sampler3D          samp3D;
layout(set = 2, binding = 0) uniform samplerCube        sampCube;
layout(set = 3, binding = 0) uniform sampler2DArray     samp2DArray;
layout(set = 4, binding = 0) uniform samplerCubeArray   sampCubeArray;

layout(set = 5, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);
    vec4 texel = vec4(0.0);

    sparseTextureOffsetClampARB(samp2D[index], vec2(0.1), ivec2(2), lodClamp, texel);
    fragColor += texel;

    sparseTextureOffsetClampARB(samp3D, vec3(0.1), ivec3(2), lodClamp, texel);
    fragColor += texel;

    sparseTextureOffsetClampARB(samp2DArray, vec3(0.1), ivec2(2), lodClamp, texel);
    fragColor += texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
