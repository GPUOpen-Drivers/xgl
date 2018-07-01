#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 16) out;

layout(location = 2) in vec4 inColor[];

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        gl_Position  = inColor[i];
        gl_PointSize = inColor[i].x;
        gl_ClipDistance[2] = inColor[i].y;
        gl_CullDistance[1] = inColor[i].z;

        gl_PrimitiveID = gl_PrimitiveIDIn;
        gl_Layer = gl_InvocationID;
        gl_ViewportIndex = gl_InvocationID;

        EmitVertex();
    }

    EndPrimitive();
}
