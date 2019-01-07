#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(location = 0) in f16vec4 fIn;
layout(location = 0, xfb_buffer = 1, xfb_offset = 24) out f16vec3 fOut1;
layout(location = 2, xfb_buffer = 0, xfb_offset = 16) out f16vec2 fOut2;

void main()
{
    fOut1 = fIn.xyz;
    fOut2 = fIn.xy;
}