#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(location = 0) in f16vec4 f16v4;

layout(location = 0) out vec2 fragColor;

void main()
{
    f16vec2 f16v2 = interpolateAtCentroid(f16v4).xy;
    f16v2 += interpolateAtSample(f16v4, 2).xy;
    f16v2 += interpolateAtOffset(f16v4, f16vec2(0.2hf)).xy;

    fragColor = f16v2;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = call <2 x float> @llpc.input.import.builtin.InterpPerspCentroid(i32 {{.*}})
; SHADERTEST: %{{[0-9]*}} = call <4 x half> @llpc.input.import.interpolant.v4f16.i32.i32.i32.i32.v2f32(i32 0, i32 0, i32 0, i32 0, <2 x float> %{{.*}})
; SHADERTEST: %{{[0-9]*}} = call <2 x float> @llpc.input.interpolate.evalij.sample(i32 {{.*}})
; SHADERTEST: %{{[0-9]*}} = call <2 x float> @llpc.input.interpolate.evalij.offset.v2f16(<2 x half> <half {{.*}}, half {{.*}}>)
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
