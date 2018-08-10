#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 16) out;

layout(location = 2) out vec4 outColor;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        outColor    = gl_in[i].gl_Position;
        outColor.x += gl_in[i].gl_PointSize;
        outColor.y += gl_in[i].gl_ClipDistance[2];
        outColor.z += gl_in[i].gl_CullDistance[2];
        outColor.w += float(gl_PrimitiveIDIn + gl_InvocationID);

        EmitVertex();
    }

    EndPrimitive();
}
