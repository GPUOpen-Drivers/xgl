#version 450

layout(set = 0, binding = 0) uniform sampler1D          samp1D;
layout(set = 1, binding = 0) uniform sampler2D          samp2D[4];
layout(set = 0, binding = 1) uniform sampler2DShadow    samp2DShadow;
layout(set = 2, binding = 0) uniform samplerCubeArray   sampCubeArray[4];
layout(set = 3, binding = 0) uniform texture3D          tex3D;
layout(set = 3, binding = 1) uniform sampler            samp;

layout(set = 4, binding = 0) uniform Uniforms
{
    int index;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    vec2 f2 = textureQueryLod(samp1D, 1.0);
    f2 += textureQueryLod(samp2D[index], vec2(0.5));
    f2 += textureQueryLod(samp2DShadow, vec2(0.4));
    f2 += textureQueryLod(sampCubeArray[index], vec3(0.6));
    f2 += textureQueryLod(sampler3D(tex3D, samp), vec3(0.7));

    fragColor = vec4(f2, f2);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
