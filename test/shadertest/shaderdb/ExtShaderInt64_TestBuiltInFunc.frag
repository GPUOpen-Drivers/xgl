#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout(std140, binding = 0) uniform Uniforms
{
    bvec3    b3;
    dvec3    d3;
    int64_t  i64;
    uint64_t u64;
    i64vec3  i64v3;
    u64vec3  u64v3;
};

layout(location = 0) out vec3 fragColor;

void main()
{
    i64vec3 i64v3_0 = abs(i64v3);
    i64v3_0 += sign(i64v3);

    u64vec3 u64v3_0 = min(u64v3, u64);
    i64v3_0 += min(i64v3, i64);

    u64v3_0 += max(u64v3, u64);
    i64v3_0 += max(i64v3, i64);

    u64v3_0 += clamp(u64v3_0, u64v3.x, u64);
    i64v3_0 += clamp(i64v3_0, i64v3.x, i64);

    u64v3_0 += mix(u64v3_0, u64v3, b3);
    i64v3_0 += mix(i64v3_0, i64v3, b3);

    u64v3_0 += doubleBitsToUint64(d3);
    i64v3_0 += doubleBitsToInt64(d3);

    dvec3 d3_0 = uint64BitsToDouble(u64v3);
    d3_0 += int64BitsToDouble(i64v3);

    fragColor = ((u64v3_0.x != 0) && (i64v3_0.y == 0)) ? vec3(d3_0) : vec3(d3);
}
