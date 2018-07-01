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
