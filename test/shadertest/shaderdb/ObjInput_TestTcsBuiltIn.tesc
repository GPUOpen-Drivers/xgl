#version 450 core

layout(vertices = 3) out;

layout(location = 1) out vec4 outColor[];

void main (void)
{
    outColor[gl_InvocationID] = gl_in[gl_InvocationID].gl_Position;
    outColor[gl_InvocationID].x += gl_in[gl_InvocationID].gl_PointSize;
    outColor[gl_InvocationID].y += gl_in[gl_InvocationID].gl_ClipDistance[2];
    outColor[gl_InvocationID].z += gl_in[gl_InvocationID].gl_CullDistance[3];

    int value = gl_PatchVerticesIn + gl_PrimitiveID;
    outColor[gl_InvocationID].w += float(value);
    outColor[gl_InvocationID].w += gl_in[gl_InvocationID].gl_Position.z;
}
