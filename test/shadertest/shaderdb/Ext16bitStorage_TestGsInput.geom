#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable
#extension GL_AMD_gpu_shader_int16: enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) in f16vec3 f16v3[];
layout(location = 1) in float16_t f16v1[];

layout(location = 2) in i16vec3 i16v3[];
layout(location = 3) in uint16_t u16v1[];

layout(location = 0) out float fv1;
layout(location = 1) out vec3 fv3;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        fv1 = f16v1[i];
        fv3 = f16v3[i];

        fv1 += u16v1[i];
        fv3 += i16v3[i];

        EmitVertex();
    }

    EndPrimitive();
}
