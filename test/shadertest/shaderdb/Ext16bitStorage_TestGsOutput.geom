#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_int16: enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in uint uv1[];

layout(location = 0) out f16vec3 f16v3;
layout(location = 1) out float16_t f16v1;

layout(location = 2) out i16vec3 i16v3;
layout(location = 3) out uint16_t u16v1;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        f16v1 = float16_t(uv1[i]);
        f16v3 = f16vec3(uv1[i]);

        u16v1 = uint16_t(uv1[i]);
        i16v3 = i16vec3(uv1[i]);

        EmitVertex();
    }

    EndPrimitive();
}
