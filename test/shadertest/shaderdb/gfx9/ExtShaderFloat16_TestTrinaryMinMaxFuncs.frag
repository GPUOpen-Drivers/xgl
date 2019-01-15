#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_shader_trinary_minmax: enable

layout(binding = 0) buffer Buffers
{
    vec3  fv3[3];
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3_1 = f16vec3(fv3[0]);
    f16vec3 f16v3_2 = f16vec3(fv3[1]);
    f16vec3 f16v3_3 = f16vec3(fv3[2]);

    f16v3_1 += min3(f16v3_1, f16v3_2, f16v3_3);
    f16v3_1 += max3(f16v3_1, f16v3_2, f16v3_3);
    f16v3_1 += mid3(f16v3_1, f16v3_2, f16v3_3);

    fragColor = f16v3_1;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
