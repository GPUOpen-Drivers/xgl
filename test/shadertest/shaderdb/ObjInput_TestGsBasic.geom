#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 16) out;

layout(location = 2) in dvec4 inData1[];
layout(location = 5) in float inData2[];

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        gl_Position.x = float(inData1[i].z / inData1[i].y);
        gl_Position.y = inData2[i];

        EmitVertex();
    }

    EndPrimitive();
}
