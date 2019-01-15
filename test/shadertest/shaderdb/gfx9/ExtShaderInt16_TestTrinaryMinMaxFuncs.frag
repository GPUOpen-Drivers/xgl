#version 450 core

#extension GL_AMD_gpu_shader_int16: enable
#extension GL_AMD_shader_trinary_minmax: enable

layout(binding = 0) buffer Buffers
{
    ivec3  iv3[3];
    uvec3  uv3[3];
};

layout(location = 0) out vec3 fragColor;

void main()
{
    i16vec3 i16v3_1 = i16vec3(iv3[0]);
    i16vec3 i16v3_2 = i16vec3(iv3[1]);
    i16vec3 i16v3_3 = i16vec3(iv3[2]);

    u16vec3 u16v3_1 = u16vec3(uv3[0]);
    u16vec3 u16v3_2 = u16vec3(uv3[1]);
    u16vec3 u16v3_3 = u16vec3(uv3[2]);

    i16vec3 i16v3 = i16vec3(0s);
    i16v3 += min3(i16v3_1, i16v3_2, i16v3_3);
    i16v3 += max3(i16v3_1, i16v3_2, i16v3_3);
    i16v3 += mid3(i16v3_1, i16v3_2, i16v3_3);

    u16vec3 u16v3 = u16vec3(0us);
    u16v3 += min3(u16v3_1, u16v3_2, u16v3_3);
    u16v3 += max3(u16v3_1, u16v3_2, u16v3_3);
    u16v3 += mid3(u16v3_1, u16v3_2, u16v3_3);

    fragColor  = i16v3;
    fragColor += u16v3;
}
// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
