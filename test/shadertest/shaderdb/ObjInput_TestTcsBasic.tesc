#version 450 core

layout(vertices = 3) out;

layout(location = 1) in float inData1[];
layout(location = 2) in dvec4 inData2[];

void main (void)
{
    gl_out[gl_InvocationID].gl_Position = vec4(inData1[gl_InvocationID]);
    gl_out[gl_InvocationID].gl_PointSize = float(inData2[gl_InvocationID].z);
}
