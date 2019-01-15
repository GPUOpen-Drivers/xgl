#version 450

layout(location = 0) in vec2 vsOut0;
layout(location = 1) in vec3 vsOut1;
layout(location = 2) in vec4 vsOut2;
layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0)  uniform sampler1D samp1D;
layout(set = 0, binding = 1)  uniform sampler2D samp2D;
layout(set = 0, binding = 2)  uniform sampler3D samp3D;
layout(set = 0, binding = 3)  uniform sampler2DRect samp2DR;
layout(set = 0, binding = 4)  uniform sampler1DShadow samp1DS;
layout(set = 0, binding = 5)  uniform sampler2DShadow samp2DS;
layout(set = 0, binding = 7)  uniform sampler2DRectShadow samp2DRS;

void main()
{
    oColor  = textureProj(samp1D, vsOut0);
    oColor += textureProj(samp2D, vsOut1);
    oColor += textureProj(samp3D, vsOut2);
    oColor += textureProj(samp2DR, vsOut1);
    oColor += vec4(textureProj(samp1DS, vsOut2), 1, 0, 1);
    oColor += vec4(textureProj(samp2DS, vsOut2), 1, 0, 1);
    oColor += vec4(textureProj(samp2DRS, vsOut2), 1, 0, 1);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
