#version 450 core

layout(location = 0) in vec4 a0;
layout(location = 1) in vec4 b0;
layout(location = 2) in vec4 c0;
layout(location = 0) out vec4 color;

void main()
{
    vec2 a = a0.xy;
    vec2 b = b0.xy;
    color = vec4(refract(a0, b0, c0.x));
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z7refractDv4_fDv4_ff(<4 x float> %{{.*}}, <4 x float> %{{.*}}, float %{{.*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
