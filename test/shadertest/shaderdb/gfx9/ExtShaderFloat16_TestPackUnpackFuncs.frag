#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(binding = 0) buffer Buffers
{
    uint u;
};

void main()
{
    f16vec2 f16v2 = unpackFloat2x16(u);
    f16v2 += f16vec2(0.25hf);
    u = packFloat2x16(f16v2);
}