#version 450
#extension GL_ARB_sparse_texture_clamp : enable

layout(set = 0, binding = 0) uniform sampler1D          samp1D[4];
layout(set = 1, binding = 0) uniform sampler2D          samp2D;
layout(set = 2, binding = 0) uniform sampler3D          samp3D;
layout(set = 3, binding = 0) uniform samplerCube        sampCube;
layout(set = 4, binding = 0) uniform sampler1DArray     samp1DArray;
layout(set = 5, binding = 0) uniform sampler2DArray     samp2DArray;
layout(set = 6, binding = 0) uniform samplerCubeArray   sampCubeArray;

layout(set = 7, binding = 0) uniform Uniforms
{
    int   index;
    float lodClamp;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(0.0);

    fragColor += textureGradClampARB(samp1D[index], 0.1, 0.2, 0.3, lodClamp);

    fragColor += textureGradClampARB(samp2D, vec2(0.1), vec2(0.2), vec2(0.3), lodClamp);

    fragColor += textureGradClampARB(samp3D, vec3(0.1), vec3(0.2), vec3(0.3), lodClamp);

    fragColor += textureGradClampARB(sampCube, vec3(0.1), vec3(0.2), vec3(0.3), lodClamp);

    fragColor += textureGradClampARB(samp1DArray, vec2(0.1), 0.2, 0.3, lodClamp);

    fragColor += textureGradClampARB(samp2DArray, vec3(0.1), vec2(0.2), vec2(0.3), lodClamp);

    fragColor += textureGradClampARB(sampCubeArray, vec4(0.1), vec3(0.2), vec3(0.3), lodClamp);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
