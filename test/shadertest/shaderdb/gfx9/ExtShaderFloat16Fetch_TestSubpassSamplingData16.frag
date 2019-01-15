#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_half_float_fetch: enable

layout(set = 3, binding = 0, input_attachment_index = 0) uniform f16subpassInput   subpass;
layout(set = 3, binding = 1, input_attachment_index = 0) uniform f16subpassInputMS subpassMS;

layout(location = 0) out vec4 fragColor;

void main()
{
    f16vec4 texel = f16vec4(0.0hf);

    texel  = subpassLoad(subpass);
    texel += subpassLoad(subpassMS, 2);

    fragColor = texel;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
