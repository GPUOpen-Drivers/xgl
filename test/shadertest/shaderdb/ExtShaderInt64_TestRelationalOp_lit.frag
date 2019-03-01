#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout(std140, binding = 0) uniform Uniforms
{
    int64_t  i64_0;
    int64_t  i64_1;

    u64vec3  u64v3_0;
    u64vec3  u64v3_1;
};

layout(location = 0) out float fragColor;

void main()
{
    bvec3 b3 = bvec3(true);

    b3 = b3.x ? equal(u64v3_0, u64v3_1) : notEqual(u64v3_0, u64v3_1);
    b3 = b3.x ? greaterThanEqual(u64v3_0, u64v3_1) : lessThanEqual(u64v3_0, u64v3_1);
    b3 = b3.x ? greaterThan(u64v3_0, u64v3_1) : lessThan(u64v3_0, u64v3_1);

    bool b1 = false;
    b1 = b1 ? (i64_0 == i64_1) : (i64_0 != i64_1);
    b1 = b1 ? (i64_0 >= i64_1) : (i64_0 <= i64_1);
    b1 = b1 ? (i64_0 > i64_1) : (i64_0 < i64_1);

    fragColor = b3.x && b1 ? 1.0 : 0.0;
}

// BEGIN_SHADERTEST
/*
; RUN: amdllpc -v %gfxip %s | FileCheck -check-prefix=SHADERTEST %s
; SHADERTEST-LABEL: {{^// LLPC}} SPIRV-to-LLVM translation results
; SHADERTEST: icmp eq <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp ne <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp uge <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp ule <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp ugt <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp ult <3 x i64> %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp eq i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp ne i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp sge i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp sle i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp sgt i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: icmp slt i64 %{{[0-9]*}}, %{{[0-9]*}}
; SHADERTEST: AMDLLPC SUCCESS
*/
// END_SHADERTEST
