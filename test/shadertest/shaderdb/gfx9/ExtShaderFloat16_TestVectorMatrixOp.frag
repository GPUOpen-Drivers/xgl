#version 450

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0, std430) buffer Buffers
{
    vec3 fv3;

    int i;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    f16vec3 f16v3 = f16vec3(fv3);

    float16_t f16 = f16v3.xy[i];
    f16v3 = f16v3 * f16;

    f16mat2x3 f16m2x3= f16mat2x3(f16v3, f16v3);
    f16m2x3 = f16m2x3 * f16;

    f16v3 += f16m2x3 * f16v3.xy;

    f16vec2 f16v2 = f16v3 * f16m2x3;

    f16mat3x2 f16m3x2 = f16mat3x2(f16v2, f16v2, f16v2);
    f16mat3 f16m3 = f16m2x3 * f16m3x2;

    fragColor = f16m3[0];
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
