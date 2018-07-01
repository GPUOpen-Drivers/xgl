#version 450 core

#extension GL_AMD_shader_explicit_vertex_parameter: enable

layout(location = 0) in __explicitInterpAMD vec2 fv2In;
layout(location = 1) in __explicitInterpAMD ivec2 iv2In;
layout(location = 2) in __explicitInterpAMD uvec2 uv2In;

layout(location = 0) out vec2 fOut;

void main()
{
    fOut  = interpolateAtVertexAMD(fv2In, 2);
    fOut += vec2(interpolateAtVertexAMD(iv2In, 1));
    fOut += vec2(interpolateAtVertexAMD(uv2In, 0));
}