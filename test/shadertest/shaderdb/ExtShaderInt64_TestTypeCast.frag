#version 450

#extension GL_ARB_gpu_shader_int64 : enable

layout(std430, binding = 0) buffer Buffers
{
    ivec3    i3;
    uvec3    u3;
    vec3     f3;
    dvec3    d3;
    i64vec3  i64v3;
    u64vec3  u64v3;
};

void main()
{
    i64v3  = i64vec3(f3);
    i64v3 += i64vec3(d3);

    u64v3  = u64vec3(f3);
    u64v3 += u64vec3(d3);

    f3  = vec3(i64v3);
    f3 += vec3(u64v3);

    d3  = dvec3(i64v3);
    d3 += dvec3(u64v3);

    i3 = ivec3(i64v3);
    i64v3 += i64vec3(i3);

    u3 = uvec3(u64v3);
    u64v3 += u64vec3(u3);
}
