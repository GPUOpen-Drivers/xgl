#version 450 core

layout(location = 0) in vec4 a;
layout(location = 0) out vec4 frag_color;

void main()
{
    vec4 fv = floor(a);
    fv.x = floor(1.5);
    frag_color = fv;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: %{{[0-9]*}} = call spir_func <4 x float> @_Z5floorDv4_f(<4 x float> %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: %{{[0-9]*}} = tail call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = tail call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: %{{[0-9]*}} = tail call float @llvm.floor.f32(float %{{[0-9]*}})
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
