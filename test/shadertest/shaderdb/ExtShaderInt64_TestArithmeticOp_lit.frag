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
    u64vec3 u64v3_0 = u64vec3(0);
    u64v3_0 += u64v3;
    u64v3_0 -= u64v3;
    u64v3_0 *= u64v3;
    u64v3_0 /= u64v3;
    u64v3_0 %= u64v3;

    int64_t i64_0 = 0;
    i64_0 += i64;
    i64_0 -= i64;
    i64_0 *= i64;
    i64_0 /= i64;
    i64_0 %= i64;

    u64v3_0.x = -i64;
    fragColor = vec3(u64v3_0);
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: add <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: sub <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: mul <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: udiv <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: urem <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: add i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: sub i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: mul i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: sdiv i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: call {{.*}} i64 @_Z4smodll
; SHADERTEST: sub nsw i64 0, %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
