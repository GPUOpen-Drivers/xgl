#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(constant_id = 100) const float16_t sf16 = 0.125hf;
layout(constant_id = 101) const float     sf   = 0.25;
layout(constant_id = 102) const double    sd   = 0.5lf;

const float  f16_to_f = float(sf16);
const double f16_to_d = float(sf16);

const float16_t f_to_f16 = float16_t(sf);
const float16_t d_to_f16 = float16_t(sd);

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor.x = f16_to_f;
    fragColor.y = float(f16_to_d);
    fragColor.z = float(f_to_f16);
    fragColor.w = float(d_to_f16);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
