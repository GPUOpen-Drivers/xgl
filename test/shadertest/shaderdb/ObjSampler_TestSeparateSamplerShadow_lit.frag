#version 450 core

layout(set = 0, binding = 0) uniform texture2D      tex2D;
layout(set = 0, binding = 1) uniform samplerShadow  sampShadow;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(textureLod(sampler2DShadow(tex2D, sampShadow), vec3(0.0), 0));
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: call {{.*}} float @spirv.image.sample.f32.2D.dref.lod

; SHADERTEST-LABEL: {{^// LLPC.*}} SPIR-V lowering results
: SHADERTEST: call float @llpc.image.sample.f32.2D.dref.lod.dimaware

; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
: SHADERTEST: call float @llvm.amdgcn.image.sample.c.l.2d.f32.f32

; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
