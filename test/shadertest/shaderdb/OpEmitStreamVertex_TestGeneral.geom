#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 32, stream = 1) out;

layout(location = 0) in vec4 colorIn[];
layout(location = 6) out vec4 colorOut;

void main ( )
{
    for (int i = 0; i < gl_in.length(); i++)
    {
        colorOut = colorIn[i];
        EmitStreamVertex(1);
    }

    EndStreamPrimitive(1);
}