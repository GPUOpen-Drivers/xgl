#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) buffer Buffers
{
    uint u;
};

void main()
{
    f16vec2 f16v2 = unpackFloat2x16(u);
    f16v2 += f16vec2(0.25hf);
    u = packFloat2x16(f16v2);
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST-LABEL: {{^// LLPC}} SPIR-V lowering results
; SHADERTEST: %{{[0-9]*}} = bitcast <4 x i8> %{{[0-9]*}} to <2 x half>
; SHADERTEST: %{{[0-9]*}} = bitcast <2 x half> %{{[0-9]*}} to <4 x i8>
; SHADERTEST-LABEL: {{^// LLPC}} pipeline patching results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
