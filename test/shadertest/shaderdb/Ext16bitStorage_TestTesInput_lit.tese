#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_int16: enable

layout(triangles) in;

layout(location = 0) in f16vec3 f16v3[];
layout(location = 1) in float16_t f16v1[];

layout(location = 2) in i16vec3 i16v3[];
layout(location = 3) in uint16_t u16v1[];

layout(location = 0) out float fv1;
layout(location = 1) out vec3 fv3;

void main(void)
{
    fv1 = f16v1[0];
    fv3 = f16v3[1];

    fv1 += u16v1[0];
    fv3 += i16v3[1];
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: call half @llpc.input.import.generic.f16{{.*}}
; SHADERTEST: call <3 x half> @llpc.input.import.generic.v3f16{{.*}}
; SHADERTEST: call i16 @llpc.input.import.generic.i16{{.*}}
; SHADERTEST: call <3 x i16> @llpc.input.import.generic.v3i16{{.*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
