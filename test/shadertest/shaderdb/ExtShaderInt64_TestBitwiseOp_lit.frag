#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout(std140, binding = 0) uniform Uniforms
{
    int64_t  i64;
    u64vec3  u64v3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    u64vec3 u64v3_0 = u64vec3(i64);
    u64v3_0 |= u64v3;
    u64v3_0 &= u64v3;
    u64v3_0 ^= u64v3;
    u64v3_0 = ~u64v3_0;

    fragColor = vec3(u64v3_0);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: or <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: and <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: xor <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: xor <3 x i64> %{{[0-9]*}}, <i64 -1, i64 -1, i64 -1>
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
