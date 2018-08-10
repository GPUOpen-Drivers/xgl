#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 16) out;

layout(location = 2) out dvec4 outData1;
layout(location = 5) out float outData2;

void main()
{
    for (int i = 0; i < gl_in.length(); ++i)
    {
        outData1 = dvec4(gl_InvocationID);
        outData2 = float(gl_PrimitiveIDIn);

        EmitVertex();
    }

    EndPrimitive();
}
