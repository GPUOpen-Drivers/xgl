#version 450 core

layout(location = 0) in float a;
layout(location = 1) in vec4 b0;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 b = sign(b0);
    frag_color = b;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call {{.*}} <4 x float> @_Z5fsignDv4_f(<4 x float> %{{.*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp ogt float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: %{{[0-9]*}} = fcmp oge float %{{.*}}, 0.000000e+00
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
