#version 450 core

layout(set = 0, binding = 0 ) uniform sampler1D samp1D;
layout(set = 0, binding = 1 ) uniform sampler2D samp2D;
layout(set = 0, binding = 2 ) uniform sampler3D samp3D;
layout(set = 0, binding = 3 ) uniform samplerCube sampCube;
layout(set = 0, binding = 4 ) uniform sampler1DArray samp1DA;
layout(set = 0, binding = 5 ) uniform sampler2DArray samp2DA;
layout(set = 0, binding = 6 ) uniform samplerCubeArray sampCubeA;
layout(set = 0, binding = 7 ) uniform sampler1DShadow samp1DS;
layout(set = 0, binding = 8 ) uniform sampler2DShadow samp2DS;
layout(set = 0, binding = 9 ) uniform samplerCubeShadow sampCubeS;
layout(set = 0, binding = 10) uniform sampler1DArrayShadow samp1DAS;
layout(set = 0, binding = 11) uniform sampler2DArrayShadow samp2DAS;
layout(set = 0, binding = 12) uniform samplerCubeArrayShadow sampCubeAS;

layout(location = 0) out vec4 oOut;

void main()
{
    vec2 temp = vec2(0);

    float coord1D = 7;
    vec2  coord2D = vec2(7, 8);
    vec3  coord3D = vec3(7, 8, 9);

    temp = textureQueryLod(samp1D, coord1D);
    temp += textureQueryLod(samp2D, coord2D);
    temp += textureQueryLod(samp3D, coord3D);
    temp += textureQueryLod(sampCube, coord3D);
    temp += textureQueryLod(samp1DA, coord1D);
    temp += textureQueryLod(samp2DA, coord2D);
    temp += textureQueryLod(sampCubeA, coord3D);
    temp += textureQueryLod(samp1DS, coord1D);
    temp += textureQueryLod(samp2DS, coord2D);
    temp += textureQueryLod(sampCubeS, coord3D);
    temp += textureQueryLod(samp1DAS, coord1D);
    temp += textureQueryLod(samp2DAS, coord2D);
    temp += textureQueryLod(sampCubeAS, coord3D);

    oOut = vec4(temp.x, temp.y, 0, 0);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
