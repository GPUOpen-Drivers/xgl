#version 450 core

#extension GL_AMD_gpu_shader_half_float: enable

layout(location = 0) in f16vec4 f16v4;

layout(location = 0) out vec2 fragColor;

void main()
{
    f16vec2 f16v2 = interpolateAtCentroid(f16v4).xy;
    f16v2 += interpolateAtSample(f16v4, 2).xy;
    f16v2 += interpolateAtOffset(f16v4, f16vec2(0.2hf)).xy;

    fragColor = f16v2;
}