#version 450 core

layout(vertices = 3) out;

layout(location = 1) in vec4 inColor[];

void main (void)
{
    gl_out[gl_InvocationID].gl_Position = inColor[gl_InvocationID];
    gl_out[gl_InvocationID].gl_PointSize = inColor[gl_InvocationID].x;
    gl_out[gl_InvocationID].gl_ClipDistance[2] = inColor[gl_InvocationID].y;
    gl_out[gl_InvocationID].gl_CullDistance[3] = inColor[gl_InvocationID].z;

    barrier();

    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[2] = 1.0;
    gl_TessLevelInner[0] = float(gl_PrimitiveID);
}
