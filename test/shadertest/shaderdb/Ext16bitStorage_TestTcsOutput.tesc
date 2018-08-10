#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_int16: enable

layout(vertices = 3) out;

layout(location = 0) in uint uv1[];

layout(location = 0) out f16vec3 f16v3[];
layout(location = 1) out float16_t f16v1[];

layout(location = 2) out i16vec3 i16v3[];
layout(location = 3) out uint16_t u16v1[];

void main(void)
{
    f16v1[gl_InvocationID] = float16_t(uv1[gl_InvocationID]);
    f16v3[gl_InvocationID] = f16vec3(uv1[gl_InvocationID]);

    u16v1[gl_InvocationID] = uint16_t(uv1[gl_InvocationID]);
    i16v3[gl_InvocationID] = i16vec3(uv1[gl_InvocationID]);

    gl_TessLevelInner[0] = 1.0;
    gl_TessLevelOuter[0] = 1.0;
}