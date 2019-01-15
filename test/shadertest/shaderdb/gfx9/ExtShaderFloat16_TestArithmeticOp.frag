#version 450

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) uniform Uniforms
{
    vec4 fv4;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    f16vec4   f16v4 = f16vec4(fv4);
    float16_t f16   = -f16v4.x;

    f16v4 += f16;
    f16v4 *= f16v4;
    f16v4 -= f16;
    f16v4 /= f16;
    f16v4++;
    f16v4--;

    fragColor = f16v4;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
